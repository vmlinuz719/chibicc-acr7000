// Linking for the ACR-7000.
//
// asm2 assembles a single file and has no object format, so "object
// files" are assembly text and linking means concatenating them, after
// a generated startup module, and assembling the result into a paper
// tape image. Lines starting with ";@" (comments to asm2) tell this
// linker about the symbols in each unit:
//
//   ;@unit <id>                    translation unit id (must be unique)
//   ;@def <label> <name>           definition of an external symbol
//   ;@ref <label> <name>           reference to an external symbol
//   ;@comm <label> <words> <name>  common (tentative) definition
//   ;@zero <label> <words>         region crt0 must clear
//   ;@fixup <word> <label> <n> <s> crt0 stores (<label> << <s>) + n at <word>
//                                  (a char pointer has s = 2, a short pointer 1)
//
// The startup module sets up the stack, clears zero-fill regions,
// applies fixups (asm2 has no address arithmetic) and calls main().
// Memory layout:
//
//   0       exception vector (PSW, CW)
//   1024    crt0, then the C library, then the program, in link order
//           ...  heap (from __heap_start upward)
//           ...  stack (downward from the top of memory)

#include "chibicc.h"

typedef struct {
  char *label;
  char *name;
  char *where;
  int size;
  bool placed;
} Symbol;

static HashMap defs;    // label -> Symbol
static HashMap commons; // label -> Symbol
static HashMap units;   // unit id -> file
static StringArray zero_entries;
static StringArray fixup_entries;
static StringArray big_bss;  // "label size" pairs placed at the end

typedef struct {
  char *label;
  char *name;
  char *where;
} Ref;

static Ref *refs;
static int nrefs;

static char *line_end(char *p) {
  while (*p && *p != '\n')
    p++;
  return p;
}

static char *read_text(char *path) {
  FILE *fp = strcmp(path, "-") ? fopen(path, "r") : stdin;
  if (!fp)
    error("cannot open %s: %s", path, strerror(errno));

  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);
  char tmp[4096];
  size_t n;
  while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0)
    fwrite(tmp, 1, n, out);
  if (fp != stdin)
    fclose(fp);
  fclose(out);
  return buf;
}

static void add_ref(char *label, char *name, char *where) {
  refs = realloc(refs, sizeof(Ref) * (nrefs + 1));
  refs[nrefs++] = (Ref){label, name, where};
}

static void define(char *label, char *name, char *where) {
  Symbol *sym = hashmap_get(&defs, label);
  if (sym)
    error("multiple definition of '%s' (in %s and %s)", name, sym->where, where);
  sym = calloc(1, sizeof(Symbol));
  *sym = (Symbol){label, name, where};
  hashmap_put(&defs, label, sym);
}

// First pass: collect symbol information from a unit.
static void scan(char *text, char *where) {
  for (char *line = text; *line;) {
    char *eol = line_end(line);
    if (!strncmp(line, ";@", 2)) {
      char *s = strndup(line + 2, eol - line - 2);
      char *kw = strtok(s, " \t");
      char *a = strtok(NULL, " \t");
      char *b = strtok(NULL, " \t");
      char *c = strtok(NULL, " \t");

      if (!kw || !a) {
        // ignore
      } else if (!strcmp(kw, "unit")) {
        char *other = hashmap_get(&units, a);
        if (other)
          error("%s and %s have the same unit id '%s'; rename one of them",
                other, where, a);
        hashmap_put(&units, a, where);
      } else if (!strcmp(kw, "def")) {
        define(a, b ? b : a, where);
      } else if (!strcmp(kw, "ref")) {
        add_ref(a, b ? b : a, where);
      } else if (!strcmp(kw, "comm") && b) {
        Symbol *sym = hashmap_get(&commons, a);
        if (!sym) {
          sym = calloc(1, sizeof(Symbol));
          *sym = (Symbol){a, c ? c : a, where};
          hashmap_put(&commons, a, sym);
        }
        sym->size = MAX(sym->size, atoi(b));
      }
    }
    line = *eol ? eol + 1 : eol;
  }
}

// Second pass: copy a unit to the output, placing common symbols and
// collecting table entries.
static void copy_unit(FILE *out, char *text) {
  for (char *line = text; *line;) {
    char *eol = line_end(line);
    fprintf(out, "%.*s\n", (int)(eol - line), line);

    if (!strncmp(line, ";@", 2)) {
      char *s = strndup(line + 2, eol - line - 2);
      char *kw = strtok(s, " \t");
      char *a = strtok(NULL, " \t");
      char *b = strtok(NULL, " \t");
      char *c = strtok(NULL, " \t");

      if (!kw || !a) {
        // ignore
      } else if (!strcmp(kw, "comm")) {
        Symbol *sym = hashmap_get(&commons, a);
        if (sym && !sym->placed && !hashmap_get(&defs, a)) {
          sym->placed = true;
          if (sym->size >= BIG_BSS) {
            strarray_push(&big_bss, format("%s %d", a, sym->size));
          } else if (sym->size > 0) {
            fprintf(out, "%s:\n            bss         %d\n", a, sym->size);
            strarray_push(&zero_entries, format("%s, %d", a, sym->size));
          } else {
            fprintf(out, "%s:\n", a);
          }
        }
      } else if (!strcmp(kw, "bigbss") && b) {
        strarray_push(&big_bss, format("%s %s", a, b));
      } else if (!strcmp(kw, "zero") && b) {
        strarray_push(&zero_entries, format("%s, %s", a, b));
      } else if (!strcmp(kw, "fixup") && b && c) {
        int64_t addend = strtoll(c, NULL, 10);
        char *d = strtok(NULL, " \t");
        strarray_push(&fixup_entries,
                      format("%s, %s, 0%lo, %s", a, b, (unsigned long)(addend & WORD_MASK),
                             d ? d : "0"));
      }
    }
    line = *eol ? eol + 1 : eol;
  }
}

static char *crt0(uint64_t stack_top) {
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);

  fprintf(out,
    "; crt0: chibicc startup module for the ACR-7000\n"
    "\n"
    "            origin      0\n"
    "            dw          $$exc, 0                ; exception vector\n"
    "\n"
    "            origin      1024\n"
    "\n"
    "$$start:    ld          sp, $$stktp\n"
    "\n"
    "            ldea        x1, $$fixtab             ; apply address fixups\n"
    "            ld          x2, 0(x1)\n"
    "            movrn.rz    x2, x2\n"
    "            jmp         .8\n"
    "            ld          x3, 1(x1)\n"
    "            ld          x4, 3(x1)\n"
    "            edit        x4, $$fsh                ; x3 <<= x4\n"
    "            add         x3, 2(x1)\n"
    "            st          x3, 0(x2)\n"
    "            ldea        x1, 4(x1)\n"
    "            jmp         .-9\n"
    "\n"
    "            ldea        x1, $$zerotab            ; clear zero-fill regions\n"
    "            ld          x2, 0(x1)\n"
    "            movrn.rz    x2, x2\n"
    "            jmp         .9\n"
    "            ld          x3, 1(x1)\n"
    "            xorr        x4, x4\n"
    "            st          x4, 0(x2)\n"
    "            incr        x2, x2\n"
    "            addi.rn     x3, x3, -1\n"
    "            jmp         .-3\n"
    "            ldea        x1, 2(x1)\n"
    "            jmp         .-10\n"
    "\n"
    "            ldea        x0, $$argv               ; exit(main(0, argv))\n"
    "            push        x0\n"
    "            xorr        x0, x0\n"
    "            push        x0\n"
    "            calls       %s\n"
    "            addea       sp, 2\n"
    "            push        ac\n"
    "            calls       %s\n"
    "            hlt\n"
    "            jmp         .-1\n"
    "\n"
    "        ; Exceptions (vector 0): report through __acr_trap(code, pc)\n"
    "\n"
    "$$exc:      ld          sp, $$stktp\n"
    "            stctl       psw1, $$xcw\n"
    "            ld          x0, $$xcw\n"
    "            movsrz      x0, x0, x0, 24\n"
    "            andi        x0, x0, 15\n"
    "            ld          x1, 32                   ; interrupted PSW\n"
    "            movmz       x1, x1, 9\n"
    "            push        x1\n"
    "            push        x0\n"
    "            calls       %s\n"
    "            hlt\n"
    "            jmp         .-1\n"
    "\n"
    "$$xcw:      dw          0\n"
    "$$stktp:    dw          0%lo\n"
    "$$argv:     dw          0\n"
    "\n"
    "        ; Runtime constants and templates for edit (variable shifts)\n"
    "\n"
    "$$sgn:      dw          0400000000000\n"
    "$$shl:      movsz       x0, x0, x0\n"
    "$$shr:      movsrz      x0, x0, x0\n"
    "$$sar:      movsr       x0, x0, x0\n"
    "$$fsh:      movsz       x3, x3, x3\n"
    "\n"
    "        ; Byte pointer fields (shift << 27) for chars 0-3 and shorts 0-1\n"
    "        ; of a word, which are stored big-endian\n"
    "\n"
    "$$bp:       dw          033000000000, 022000000000, 011000000000, 0\n"
    "$$hp:       dw          022000000000, 0\n"
    "$$s27:      dw          033000000000\n"
    "$$s18:      dw          022000000000\n"
    "$$s9:       dw          011000000000\n",
    acr_global_label("main"), acr_global_label("exit"),
    acr_global_label("__acr_trap"), (unsigned long)stack_top);

  fclose(out);
  return buf;
}

//
// Library section selection
//
// Units compiled by chibicc mark each function and data object with
// ";@sect <label>". Program units are always linked whole; of the C
// library, only the sections reachable from the program are kept.
//

typedef struct {
  char *label;
  char *start;  // text of the section
  char *end;
  bool keep;
} Section;

static Section *sections;
static int nsections;
static HashMap section_of;  // label -> Section index + 1

static bool is_label_char(char c) {
  return isalnum((unsigned char)c) || c == '_' || c == '$' || c == '?' || c == '.';
}

// Call fn for every word in the code part (before any ';' comment) of
// the text, and for the labels on ";@uses" lines.
static void for_each_symbol(char *start, char *end, void (*fn)(char *, int)) {
  for (char *line = start; line < end;) {
    char *eol = line_end(line);
    if (eol > end)
      eol = end;
    char *stop = line;
    if (!strncmp(line, ";@uses", 6))
      stop = eol;
    else
      while (stop < eol && *stop != ';')
        stop++;

    char *p = (stop == eol && !strncmp(line, ";@uses", 6)) ? line + 6 : line;
    while (p < stop) {
      while (p < stop && !is_label_char(*p))
        p++;
      char *q = p;
      while (q < stop && is_label_char(*q))
        q++;
      if (q > p) {
        char *w = p;
        while (w < q && *w == '.')
          w++;
        if (q > w)
          fn(w, q - w);
      }
      p = q;
    }
    line = *eol ? eol + 1 : eol;
  }
}

static StringArray worklist;

static void mark_word(char *word, int len) {
  long idx = (long)hashmap_get2(&section_of, word, len);
  if (idx && !sections[idx - 1].keep) {
    sections[idx - 1].keep = true;
    strarray_push(&worklist, strndup(word, len));
  }
}

static void split_library(char *text) {
  Section *cur = NULL;
  for (char *line = text; *line;) {
    char *eol = line_end(line);
    if (!strncmp(line, ";@sect ", 7)) {
      if (cur)
        cur->end = line;
      sections = realloc(sections, sizeof(Section) * (nsections + 1));
      cur = &sections[nsections++];
      *cur = (Section){strndup(line + 7, eol - line - 7), line, NULL, false};
      hashmap_put(&section_of, cur->label, (void *)(long)nsections);
    } else if (cur && (!strncmp(line, ";@def ", 6) || !strncmp(line, ";@comm ", 7))) {
      char *a = line + (line[2] == 'd' ? 6 : 7);
      char *b = a;
      while (b < eol && *b != ' ')
        b++;
      hashmap_put2(&section_of, a, b - a, (void *)(long)nsections);
    }
    line = *eol ? eol + 1 : eol;
  }
  if (cur)
    cur->end = line_end(cur->start) + strlen(line_end(cur->start));
}

// Returns the library text with unused sections removed.
static char *select_library(char *lib, StringArray *program, char **roots) {
  split_library(lib);
  if (nsections == 0)
    return lib;

  for (int i = 0; roots[i]; i++)
    mark_word(roots[i], strlen(roots[i]));
  for (int i = 0; i < program->len; i++)
    for_each_symbol(program->data[i], program->data[i] + strlen(program->data[i]), mark_word);

  while (worklist.len > 0) {
    char *label = worklist.data[--worklist.len];
    Section *sec = &sections[(long)hashmap_get(&section_of, label) - 1];
    for_each_symbol(sec->start, sec->end, mark_word);
  }

  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);
  fwrite(lib, 1, sections[0].start - lib, out);
  for (int i = 0; i < nsections; i++)
    if (sections[i].keep)
      fwrite(sections[i].start, 1, sections[i].end - sections[i].start, out);
  fclose(out);
  return buf;
}

//
// asm2 limits
//
// asm2 keeps at most 4096 labels and 4096 unresolved forward references
// and assembles into a 32768-word work area (each origin/bss directive
// costs two words of it), none of which it checks. Check them here.
//

#define ASM2_MAX_LABELS 4096
#define ASM2_MAX_THUNKS 4096
#define ASM2_WORK_AREA 32768

typedef struct {
  char *text;
  int len;
} Word;

// Split a line into whitespace/comma separated words, stopping at a
// comment. Returns the number of words.
static int split_words(char *line, char *eol, Word *words, int max) {
  int n = 0;
  char *p = line;
  while (p < eol && n < max) {
    while (p < eol && (*p == ' ' || *p == '\t' || *p == ','))
      p++;
    if (p >= eol || *p == ';')
      break;
    char *q = p;
    if (*p == '"') {
      q++;
      while (q < eol && *q != '"')
        q++;
      if (q < eol)
        q++;
    } else {
      while (q < eol && *q != ' ' && *q != '\t' && *q != ',' && *q != ';')
        q++;
    }
    words[n++] = (Word){p, q - p};
    p = q;
  }
  return n;
}

// The label an operand refers to, if any.
static char *operand_label(Word w, int *len) {
  char *p = w.text, *end = w.text + w.len;
  if (p < end && *p == '@')
    p++;
  if (p < end && strchr("._+=", *p))
    p++;
  if (p >= end || isdigit((unsigned char)*p) || *p == '-' || *p == '#' || *p == '"' || *p == '(')
    return NULL;
  char *q = p;
  while (q < end && *q != '(')
    q++;
  *len = MIN(q - p, 10);
  return p;
}

// Parse a number the way asm2 does: leading 0 octal, # hex, else decimal.
static long asm2_number(Word w) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%.*s", w.len, w.text);
  if (buf[0] == '#')
    return strtol(buf + 1, NULL, 16);
  if (buf[0] == '0')
    return strtol(buf, NULL, 8);
  return strtol(buf, NULL, 10);
}

static void check_asm2_limits(char *text, uint64_t stack_top) {
  // Pass 1: label definitions.
  HashMap labels = {};
  int nlabels = 0;
  for (char *line = text; *line;) {
    char *eol = line_end(line);
    Word w[64];
    int n = split_words(line, eol, w, 64);
    if (n > 0 && w[0].text[w[0].len - 1] == ':') {
      int len = MIN(w[0].len - 1, 10);
      if (!hashmap_get2(&labels, w[0].text, len)) {
        hashmap_put2(&labels, w[0].text, len, (void *)1);
        nlabels++;
      }
    }
    line = *eol ? eol + 1 : eol;
  }

  // Pass 2: forward references, work area use and label addresses.
  HashMap defined = {};
  HashMap pending = {};
  long thunks = 0, max_thunks = 0, words = 2;
  bool in_list = false;
  long pc = 0;
  HashMap address = {};   // label -> pc + 1
  HashMap insn_refs = {}; // labels used in instruction address fields
  StringArray insn_ref_list = {};

  for (char *line = text; *line;) {
    char *eol = line_end(line);
    Word w[64];
    int n = split_words(line, eol, w, 64);
    int i = 0;

    if (n > 0 && w[0].text[w[0].len - 1] == ':') {
      int len = MIN(w[0].len - 1, 10);
      hashmap_put2(&address, w[0].text, len, (void *)(pc + 1));
      hashmap_put2(&defined, w[0].text, len, (void *)1);
      thunks -= (long)hashmap_get2(&pending, w[0].text, len);
      hashmap_put2(&pending, w[0].text, len, (void *)0);
      i = 1;
    }

    if (i < n) {
      Word op = w[i];
      bool is_list = in_list;
      int first = i;
      long before = words;
      if (!in_list) {
        first = i + 1;
        if (op.len == 6 && !strncmp(op.text, "origin", 6)) {
          words += 2;
          if (first < n)
            pc = asm2_number(w[first]) - 2;
        } else if (op.len == 3 && !strncmp(op.text, "bss", 3)) {
          words += 2;
          if (first < n)
            pc += asm2_number(w[first]) - 2;
        } else if (op.len == 2 && !strncmp(op.text, "dw", 2)) {
          words += n - first, is_list = true;
        } else if ((op.len == 2 && !strncmp(op.text, "ds", 2)) ||
                   (op.len == 3 && !strncmp(op.text, "dsn", 3))) {
          int chars = 0;
          for (int j = first; j < n; j++)
            chars += (w[j].text[0] == '"') ? w[j].len - 2 : 1;
          words += (chars + 4) / 5 + 1;
          is_list = true;
        } else {
          words++;
        }
      } else {
        words += n - first;
      }
      pc += words - before;

      for (int j = first; j < n; j++) {
        int len;
        char *l = operand_label(w[j], &len);
        if (l && hashmap_get2(&labels, l, len) && !hashmap_get2(&defined, l, len)) {
          hashmap_put2(&pending, l, len, (void *)((long)hashmap_get2(&pending, l, len) + 1));
          thunks++;
        }
        // dw words are 36 bits wide; everything else is an 18-bit field.
        if (l && !is_list && !hashmap_get2(&insn_refs, l, len)) {
          hashmap_put2(&insn_refs, l, len, (void *)1);
          strarray_push(&insn_ref_list, strndup(l, len));
        }
      }
      max_thunks = MAX(max_thunks, thunks);

      // A list continues on the next line if this one ends with a comma.
      char *last = eol;
      char *semi = memchr(line, ';', eol - line);
      if (semi)
        last = semi;
      while (last > line && (last[-1] == ' ' || last[-1] == '\t'))
        last--;
      in_list = is_list && last > line && last[-1] == ',';
    }
    line = *eol ? eol + 1 : eol;
  }

  if (getenv("CHIBICC_ACR_STATS"))
    fprintf(stderr, "asm2 usage: %d labels, %ld forward references, %ld words\n",
            nlabels, max_thunks, words);

  bool fail = false;
  if (nlabels > ASM2_MAX_LABELS) {
    fprintf(stderr, "program too large for asm2: %d labels (asm2 supports %d)\n",
            nlabels, ASM2_MAX_LABELS);
    fail = true;
  }
  if (max_thunks > ASM2_MAX_THUNKS) {
    fprintf(stderr, "program too large for asm2: %ld unresolved forward references "
            "at once (asm2 supports %d); defining functions before their use helps\n",
            max_thunks, ASM2_MAX_THUNKS);
    fail = true;
  }
  // Leave at least 1024 words of stack.
  if (pc + 1024 > (long)stack_top) {
    fprintf(stderr, "program too large: it needs %ld words of memory, but the "
            "stack starts at %lu\n", pc, (unsigned long)stack_top);
    fail = true;
  }

  // Absolute address fields are 18-bit signed.
  for (int i = 0; i < insn_ref_list.len; i++) {
    long addr = (long)hashmap_get(&address, insn_ref_list.data[i]) - 1;
    if (addr > 0377777) {
      fprintf(stderr, "program too large: %s is at word %ld, beyond the reach "
              "of 18-bit addresses (0377777)\n", insn_ref_list.data[i], addr);
      fail = true;
      break;
    }
  }
  if (words > ASM2_WORK_AREA - 2) {
    fprintf(stderr, "program too large for asm2: about %ld words (asm2's work area "
            "holds %d)\n", words, ASM2_WORK_AREA);
    fail = true;
  }
  if (fail)
    exit(1);
}

// Show the source line an asm2 error refers to.
static void report_asm_error(char *asm_path, char *msg) {
  int line_no = 0;
  if (sscanf(msg, "Error on line %d", &line_no) != 1) {
    fprintf(stderr, "%s", msg);
    return;
  }

  fprintf(stderr, "asm2: error near line %d of the linked assembly:\n", line_no);
  char *text = read_text(asm_path);
  int n = 1;
  for (char *line = text; *line; n++) {
    char *eol = line_end(line);
    if (line_no - 2 <= n && n <= line_no + 1)
      fprintf(stderr, "  %6d  %.*s\n", n, (int)(eol - line), line);
    line = *eol ? eol + 1 : eol;
  }
}

static char *find_asm2(char *argv0) {
  char *env = getenv("ACR_ASM2");
  if (env)
    return env;
  char *path = format("%s/asm2", dirname(strdup(argv0)));
  if (file_exists(path))
    return path;
  return "asm2";
}

void acr_link(StringArray *inputs, char *output, char *argv0,
              char *libc, uint64_t stack_top, char *keep_asm) {
  StringArray texts = {};
  StringArray names = {};

  StringArray program = {};
  for (int i = 0; i < inputs->len; i++)
    strarray_push(&program, read_text(inputs->data[i]));

  strarray_push(&texts, crt0(stack_top));
  strarray_push(&names, "<crt0>");
  if (libc) {
    if (!file_exists(libc))
      error("C library not found: %s", libc);
    char *roots[] = {
      acr_global_label("main"), acr_global_label("exit"),
      acr_global_label("__acr_trap"), NULL,
    };
    strarray_push(&texts, select_library(read_text(libc), &program, roots));
    strarray_push(&names, libc);
  }
  for (int i = 0; i < inputs->len; i++) {
    strarray_push(&texts, program.data[i]);
    strarray_push(&names, inputs->data[i]);
  }

  for (int i = 0; i < texts.len; i++)
    scan(texts.data[i], names.data[i]);

  char *heap = acr_global_label("__heap_start");
  define(heap, "__heap_start", "<linker>");

  bool undefined = false;
  for (int i = 0; i < nrefs; i++) {
    if (hashmap_get(&defs, refs[i].label) || hashmap_get(&commons, refs[i].label))
      continue;
    if (strcmp(refs[i].where, "<crt0>") == 0)
      continue;
    fprintf(stderr, "%s: undefined reference to '%s'\n", refs[i].where, refs[i].name);
    undefined = true;
  }
  if (!hashmap_get(&defs, acr_global_label("main")))
    error("undefined reference to 'main'");
  if (undefined)
    exit(1);

  char *asm_path = keep_asm;
  if (!asm_path) {
    asm_path = strdup("/tmp/chibicc-link-XXXXXX");
    int fd = mkstemp(asm_path);
    if (fd == -1)
      error("mkstemp failed: %s", strerror(errno));
    close(fd);
  }

  FILE *out = fopen(asm_path, "w");
  if (!out)
    error("cannot open %s: %s", asm_path, strerror(errno));

  for (int i = 0; i < texts.len; i++) {
    fprintf(out, "\n;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n");
    fprintf(out, "; %s\n\n", names.data[i]);
    copy_unit(out, texts.data[i]);
  }

  fprintf(out, "\n;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n");
  fprintf(out, "; <linker>\n\n");
  for (int i = 0; i < big_bss.len; i++) {
    char label[64];
    int size;
    sscanf(big_bss.data[i], "%63s %d", label, &size);
    strarray_push(&zero_entries, format("%s, %d", label, size));
  }
  fprintf(out, "$$zerotab:\n");
  for (int i = 0; i < zero_entries.len; i++)
    fprintf(out, "            dw          %s\n", zero_entries.data[i]);
  fprintf(out, "            dw          0, 0\n");
  fprintf(out, "$$fixtab:\n");
  for (int i = 0; i < fixup_entries.len; i++)
    fprintf(out, "            dw          %s\n", fixup_entries.data[i]);
  fprintf(out, "            dw          0\n");
  for (int i = 0; i < big_bss.len; i++) {
    char label[64];
    int size;
    sscanf(big_bss.data[i], "%63s %d", label, &size);
    fprintf(out, "%s:\n            bss         %d\n", label, size);
  }
  fprintf(out, ";@def %s __heap_start\n%s:\n", heap, heap);
  fclose(out);

  check_asm2_limits(read_text(asm_path), stack_top);

  // Assemble.
  char *asm2 = find_asm2(argv0);
  char *errpath = strdup("/tmp/chibicc-asm2-XXXXXX");
  int errfd = mkstemp(errpath);
  if (errfd == -1)
    error("mkstemp failed: %s", strerror(errno));

  FILE *tape = fopen(output, "wb");
  if (!tape)
    error("cannot open output file: %s: %s", output, strerror(errno));

  pid_t pid = fork();
  if (pid == 0) {
    dup2(fileno(tape), 1);
    dup2(errfd, 2);
    execlp(asm2, asm2, asm_path, (char *)NULL);
    fprintf(stderr, "exec failed: %s: %s\n", asm2, strerror(errno));
    _exit(127);
  }
  fclose(tape);

  int status;
  waitpid(pid, &status, 0);
  close(errfd);

  char *msg = read_text(errpath);
  unlink(errpath);

  bool failed = !WIFEXITED(status) || WEXITSTATUS(status) != 0;
  if (!failed && strstr(msg, "unresolved")) {
    fprintf(stderr, "asm2: %s", msg);
    failed = true;
  } else if (failed) {
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
      fprintf(stderr, "%s", msg);
    else if (WIFSIGNALED(status))
      fprintf(stderr, "asm2 crashed (signal %d); the program may be too large "
              "for its 32K-word work area\n", WTERMSIG(status));
    else
      report_asm_error(asm_path, msg);
  }

  if (!keep_asm)
    unlink(asm_path);
  if (failed) {
    unlink(output);
    exit(1);
  }
}
