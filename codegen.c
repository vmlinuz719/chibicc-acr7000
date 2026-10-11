// Code generator for the ACR-7000, a 36-bit word-addressed machine.
//
// Output is assembly for asm2 (acr7000/not_vibe_code/asm2.c). Like the
// original x86-64 backend this is a simple stack machine: the value of an
// expression is computed into X0, and intermediate values are pushed onto
// the hardware stack.
//
// Data model: chars are 9 bits and shorts 18, packed big-endian four or
// two to a word; ints, longs and pointers are a word (sizes count chars,
// so sizeof(int) == 4). Pointers to word-aligned types are word
// addresses, short pointers word << 1 | half and char (and void)
// pointers word << 2 | quarter (see ptr_unit in type.c). Registers hold
// chars and shorts sign- or zero-extended to 36 bits. Every variable
// starts a word; a lone char or short sits at the low end of its word.
// Floating point is not supported; any attempt to generate
// floating-point code is an error.
//
// Registers:
//   X0       expression result / primary operand
//   X1       secondary operand
//   X2-X5    scratch (copies, bit-fields, alloca, byte pointers)
//   AC MQ XY multiply/divide operands; AC holds function return values
//   AP       frame pointer
//   SP       stack pointer (stacks grow downward)
//
// Calling convention: the caller pushes arguments right to left (so the
// first argument ends up at the lowest address) and executes "calls f".
// A function's label addresses its register save mask word ("save ap"),
// so after calls the stack holds the return address, the mask and the
// caller's AP, followed by the arguments. Functions returning a struct or
// union receive a pointer to a caller-provided buffer as a hidden first
// argument and return that pointer. The caller pops its arguments. Every
// argument takes a whole number of words; frame offsets are in words.
//
//        ...
//        arg 1               3+n(ap)
//        arg 0               3(ap)
//        saved AP            2(ap)
//        save mask           1(ap)
//        return address      0(ap)  <- AP
//        locals             -1(ap) ...
//
// Labels: asm2 keeps only 10 significant characters and treats a leading
// digit, '_', '.', '+', '=', '@' or '#' specially, so C names are mapped
// to assembler labels as follows:
//   - external symbols keep their name if it is a valid label of at most
//     10 characters, otherwise "<prefix>$<hash>" (see acr_global_label)
//   - static symbols get "<prefix>?<unit><n>"
//   - compiler-internal labels get "$<unit><n>" or "$<unit>.<n>"
// where <unit> is a three-character id unique to the translation unit.
// Runtime support labels supplied by the linker start with "$$".
//
// asm2 has no linker, so the driver links by concatenating assembly
// files. Lines starting with ";@" carry the information it needs (see
// main.c): symbol definitions and references, common symbols, zero-fill
// regions and address constants with addends.

#include "chibicc.h"

static FILE *output_file;
static int depth;
static Obj *current_fn;
static char *ret_label;

static char *unit;
static int static_count;
static HashMap labels;     // C name -> label
static HashMap cnames;     // global label -> C name
static HashMap defined;    // global labels defined in this unit
static StringArray refs;   // global labels referenced by this unit
static HashMap referenced;
static HashMap used;       // every label referenced so far
static HashMap sect_uses;  // labels referenced by the current section
static StringArray sect_list;
static Node *tail_return;  // a return statement at the very end of a function

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

__attribute__((format(printf, 1, 2)))
static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}

// Emit one instruction: "op args" in the conventional columns.
__attribute__((format(printf, 2, 3)))
static void emit(char *op, char *fmt, ...) {
  fprintf(output_file, "            %-11s ", op);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}

static void emit0(char *op) {
  fprintf(output_file, "            %s\n", op);
}

static void emit_label(char *label) {
  println("%s:", label);
}

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  emit("push", "x0");
  depth++;
}

static void pop(char *reg) {
  emit("pop", "%s", reg);
  depth--;
}

noreturn static void no_float(Token *tok) {
  error_tok(tok, "floating-point is not supported on the ACR-7000");
}

int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

//
// Labels
//


static uint64_t fnv1a(uint64_t h, char *p, size_t len) {
  for (size_t i = 0; i < len; i++) {
    h ^= (unsigned char)p[i];
    h *= 0x100000001b3ULL;
  }
  return h;
}

static char *base36(uint64_t val, int width) {
  static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  char buf[16];
  int len = 0;
  do {
    buf[len++] = digits[val % 36];
    val /= 36;
  } while (val || len < width);

  char *s = calloc(1, len + 1);
  for (int i = 0; i < len; i++)
    s[i] = buf[len - 1 - i];
  return s;
}

static bool is_label_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

// Up to `max` alphanumeric characters of `name`, starting with a letter.
static char *label_prefix(char *name, int max) {
  char *buf = calloc(1, max + 2);
  int len = 0;
  for (char *p = name; *p && len < max; p++)
    if (isalnum((unsigned char)*p) && (*p & 0x80) == 0)
      buf[len++] = *p;
  if (len == 0 || isdigit((unsigned char)buf[0])) {
    memmove(buf + 1, buf, len);
    buf[0] = 'g';
    buf[max] = '\0';
  }
  return buf;
}

// The assembler label of an external C symbol. This is a pure function
// of the name so that separately compiled units agree on it.
char *acr_global_label(char *name) {
  int len = strlen(name);
  bool ok = len <= 10 && isalpha((unsigned char)name[0]);
  for (int i = 0; ok && i < len; i++)
    ok = is_label_char(name[i]) && (name[i] & 0x80) == 0;
  if (ok)
    return name;

  uint64_t h = fnv1a(0xcbf29ce484222325ULL, name, len) % (36ULL * 36 * 36 * 36 * 36);
  return format("%s$%s", label_prefix(name, 4), base36(h, 5));
}

// A three-character id for a translation unit, derived from its path
// and contents.
char *acr_unit_id(char *path) {
  uint64_t h = fnv1a(0xcbf29ce484222325ULL, path, strlen(path));
  File **files = get_input_files();
  for (int i = 0; files[i]; i++)
    if (!strcmp(files[i]->name, path) && files[i]->contents)
      h = fnv1a(h, files[i]->contents, strlen(files[i]->contents));
  return base36(h % (36 * 36 * 36), 3);
}

// Internal labels created by the parser are named ".L..<n>".
static bool is_internal(char *name) {
  return !strncmp(name, ".L..", 4);
}

static char *internal_label(char *name) {
  return format("$%s%s", unit, base36(strtol(name + 4, NULL, 10), 1));
}

// Labels that only the code generator jumps to; their definitions can
// be dropped if nothing ends up jumping to them.
static HashMap prunable;

// Labels created by the code generator itself.
static char *new_label(void) {
  char *label = format("$%s.%s", unit, base36(count(), 1));
  hashmap_put(&prunable, label, label);
  return label;
}

static char *static_label(char *name) {
  char *n = base36(static_count++, 1);
  int room = 10 - 1 - 3 - strlen(n);
  return format("%s?%s%s", label_prefix(name, MIN(room, 4)), unit, n);
}

// Map a C-level name (variable, function or parser label) to its label.
static char *label_of(char *name) {
  char *label = hashmap_get(&labels, name);
  if (label)
    return label;

  if (is_internal(name))
    label = internal_label(name);
  else
    label = acr_global_label(name);
  hashmap_put(&labels, name, label);
  return label;
}

static void note_ref(char *label) {
  if (!hashmap_get(&cnames, label) || hashmap_get(&referenced, label))
    return;
  hashmap_put(&referenced, label, label);
  strarray_push(&refs, label);
}

// Label of a symbol, recording external references for the linker.
static char *sym(char *name) {
  char *label = label_of(name);
  hashmap_put(&used, label, label);
  if (!hashmap_get(&sect_uses, label)) {
    hashmap_put(&sect_uses, label, label);
    strarray_push(&sect_list, label);
  }
  note_ref(label);
  return label;
}

// Every function and data object is a section for the linker, which
// drops library sections that nothing uses. A section lists the labels
// it refers to.
static void begin_section(char *label) {
  println(";@sect %s", label);
  sect_uses = (HashMap){};
  sect_list = (StringArray){};
}

static void end_section(void) {
  for (int i = 0; i < sect_list.len; i += 8) {
    fprintf(output_file, ";@uses");
    for (int j = i; j < sect_list.len && j < i + 8; j++)
      fprintf(output_file, " %s", sect_list.data[j]);
    fprintf(output_file, "\n");
  }
}

static void assign_labels(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (hashmap_get(&labels, var->name))
      continue;

    char *label;
    if (is_internal(var->name)) {
      label = internal_label(var->name);
    } else if (var->is_static) {
      label = static_label(var->name);
    } else {
      label = acr_global_label(var->name);
      char *other = hashmap_get(&cnames, label);
      if (other && strcmp(other, var->name))
        error("symbols '%s' and '%s' map to the same assembler label '%s'",
              other, var->name, label);
      hashmap_put(&cnames, label, var->name);
    }
    hashmap_put(&labels, var->name, label);
  }
}

//
// Instruction helpers
//

static bool fits_imm13(int64_t v) {
  return -4096 <= v && v <= 4095;
}

static bool fits_disp18(int64_t v) {
  return -131072 <= v && v <= 131071;
}

static char *disp(int64_t off, char *reg) {
  if (!fits_disp18(off))
    error("offset %ld out of range for an 18-bit displacement", (long)off);
  return format("%ld(%s)", (long)off, reg);
}

// Load a constant into a register.
static void load_imm(char *reg, int64_t val) {
  val = wrap_word(val, false);

  if (fits_disp18(val)) {
    emit("ldea", "%s, %ld", reg, (long)val);
    return;
  }

  // ldeas loads EA << 17. EA is an 18-bit field sign-extended to 36
  // bits, so the high part is usable when its bits 17 and 18 agree.
  uint64_t u = (uint64_t)val & WORD_MASK;
  uint64_t hi = u >> 17;      // 19 bits
  uint64_t lo = u & 0377777;  // 17 bits
  if (((hi >> 17) & 1) == ((hi >> 18) & 1)) {
    int64_t d = hi & 0777777;
    if (d & 0400000)
      d -= 01000000;
    emit("ldeas", "%s, %ld", reg, (long)d);
    if (lo)
      emit("addea", "%s, %ld", reg, (long)lo);
    return;
  }

  // Otherwise use an inline literal.
  emit("ld", "%s, .2", reg);
  emit("jmp", ".2");
  emit("dw", "0%lo", (unsigned long)u);
}

// Jump to `label` if X0 is zero / nonzero.
static void jump_if_zero(char *label) {
  emit("movrn.rz", "x0, x0");
  emit("jmp", "%s", label);
}

static void jump_if_nonzero(char *label) {
  emit("movrn.rn", "x0, x0");
  emit("jmp", "%s", label);
}

// X0 = carry flag.
static void carry_to_x0(void) {
  emit("xorrt", "x0, x0, 1");
}

// X0 = (X0 == 0) / (X0 != 0).
static void set_if_zero(void) {
  emit("subizn", "x0, x0, 0");
  carry_to_x0();
}

static void set_if_nonzero(void) {
  emit("subisn", "x0, x0, 0");
  carry_to_x0();
}

// Shift X0 by a constant.
static void shift_left(int n) {
  if (n)
    emit("movsz", "x0, x0, x0, %d", n);
}

static void shift_right(int n, bool is_unsigned) {
  if (!n)
    return;
  if (is_unsigned) {
    emit("movsrz", "x0, x0, x0, %d", n);
  } else {
    emit("movrtzn", "x0, x0, 1"); // carry = sign bit
    emit("movsr", "x0, x0, x0, %d", n);
  }
}

//
// Expressions
//

static int words(int size) {
  return (size + WORD_CHARS - 1) / WORD_CHARS;
}

static int log2_unit(int unit) {
  return unit == 4 ? 0 : unit == 2 ? 1 : 2;
}

// Large global objects may lie beyond the reach of 18-bit address fields
// (they are placed at the end of the program), so their address is
// loaded from a word holding it.
static bool is_far(Obj *var) {
  return !var->is_local && var->ty->kind != TY_FUNC &&
         var->ty->size >= BIG_BSS * WORD_CHARS;
}

static HashMap far_words;     // object label -> address word label
static StringArray far_list;  // address word labels to emit

static char *far_word(Obj *var) {
  char *target = sym(var->name);
  char *word = hashmap_get(&far_words, target);
  if (!word) {
    word = format("$%s.%s", unit, base36(count(), 1));
    hashmap_put(&far_words, target, word);
    strarray_push(&far_list, target);
  }
  hashmap_put(&used, word, word);
  if (!hashmap_get(&sect_uses, word)) {
    hashmap_put(&sect_uses, word, word);
    strarray_push(&sect_list, word);
  }
  return word;
}

static bool is_scalar(Type *ty) {
  return ty && (is_integer(ty) || ty->kind == TY_PTR);
}

// Word-sized scalars are accessed with ordinary memory references;
// chars and shorts with the byte instructions.
static bool is_word_scalar(Type *ty) {
  return is_scalar(ty) && ty->size == WORD_CHARS;
}

// The address field of the (first) word of a variable.
static char *var_operand(Obj *var) {
  if (var->is_local)
    return disp(var->offset, "ap");
  return sym(var->name);
}

// Reduce a value to `bits` bits, sign-extended unless unsigned.
int64_t wrap_bits(int64_t val, int bits, bool is_unsigned) {
  uint64_t mask = (bits >= 64) ? ~0ULL : (1ULL << bits) - 1;
  uint64_t u = (uint64_t)val & mask;
  if (!is_unsigned && bits < 64 && (u >> (bits - 1)))
    return (int64_t)(u | ~mask);
  return u;
}

int64_t wrap_word(int64_t val, bool is_unsigned) {
  return wrap_bits(val, WORD_BITS, is_unsigned);
}

// The value an object of type `ty` holds when assigned `val`.
int64_t wrap_type(int64_t val, Type *ty) {
  if (ty->kind == TY_BOOL)
    return wrap_word(val, true) != 0;
  if (is_integer(ty))
    return wrap_bits(val, MIN(ty->size, WORD_CHARS) * CHAR_BITS, ty->is_unsigned);
  return wrap_word(val, true);
}

// Returns true if every value of type `from` is held in a register
// exactly as it would be as a `to`, so that no conversion code is needed.
// Registers hold chars and shorts sign- or zero-extended to 36 bits.
static bool same_value(Type *from, Type *to) {
  if (to->kind == TY_BOOL)
    return from->kind == TY_BOOL;
  if (is_pointer_like(from) && is_pointer_like(to))
    return value_unit(from) == value_unit(to);
  if (!is_integer(to))
    return true;
  if (to->size >= WORD_CHARS)
    return true;
  if (!is_integer(from))
    return false;
  if (from->kind == TY_BOOL)
    return true;
  int f = from->size, t = to->size;
  if (from->is_unsigned)
    return t > f || (t == f && to->is_unsigned);
  return !to->is_unsigned && t >= f;
}

static bool is_noop_cast(Node *node) {
  return node->kind == ND_CAST && is_scalar(node->ty) &&
         is_scalar(node->lhs->ty) && same_value(node->lhs->ty, node->ty);
}

static Node *strip_casts(Node *node) {
  while (is_noop_cast(node))
    node = node->lhs;
  return node;
}

// Evaluate an integer expression at compile time if it is constant.
static bool eval_const(Node *node, int64_t *val) {
  if (!is_scalar(node->ty))
    return false;

  int64_t a, b;
  switch (node->kind) {
  case ND_NUM:
    *val = node->val;
    break;
  case ND_CAST:
    if (!is_scalar(node->lhs->ty) || !eval_const(node->lhs, &a))
      return false;
    if (is_pointer_like(node->lhs->ty) && is_pointer_like(node->ty))
      a = (int64_t)((uint64_t)wrap_word(a, true) * value_unit(node->lhs->ty) /
                    value_unit(node->ty));
    *val = a;
    break;
  case ND_NEG:
    if (!eval_const(node->lhs, &a))
      return false;
    *val = -a;
    break;
  case ND_BITNOT:
    if (!eval_const(node->lhs, &a))
      return false;
    *val = ~a;
    break;
  case ND_NOT:
    if (!is_scalar(node->lhs->ty) || !eval_const(node->lhs, &a))
      return false;
    *val = !wrap_word(a, true);
    break;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
  case ND_SHL:
  case ND_SHR:
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE: {
    if (!is_scalar(node->lhs->ty) || !is_scalar(node->rhs->ty) ||
        !eval_const(node->lhs, &a) || !eval_const(node->rhs, &b))
      return false;

    // Operands of comparisons and divisions are interpreted according
    // to the (common) operand type.
    bool u = node->lhs->ty->is_unsigned;
    a = wrap_word(a, u);
    b = wrap_word(b, u);
    uint64_t ua = a, ub = b;

    switch (node->kind) {
    case ND_ADD: *val = a + b; break;
    case ND_SUB: *val = a - b; break;
    case ND_MUL: *val = ua * ub; break;
    case ND_DIV:
    case ND_MOD:
      if (b == 0 || (!u && b == -1))
        return false;
      if (node->kind == ND_DIV)
        *val = u ? (int64_t)(ua / ub) : a / b;
      else
        *val = u ? (int64_t)(ua % ub) : a % b;
      break;
    case ND_BITAND: *val = a & b; break;
    case ND_BITOR: *val = a | b; break;
    case ND_BITXOR: *val = a ^ b; break;
    case ND_SHL:
    case ND_SHR:
      if (ub >= WORD_BITS)
        return false;
      if (node->kind == ND_SHL)
        *val = ua << ub;
      else
        *val = u ? (int64_t)(ua >> ub) : a >> b;
      break;
    case ND_EQ: *val = a == b; break;
    case ND_NE: *val = a != b; break;
    case ND_LT: *val = u ? ua < ub : a < b; break;
    case ND_LE: *val = u ? ua <= ub : a <= b; break;
    }
    break;
  }
  case ND_LOGAND:
  case ND_LOGOR:
    if (!is_scalar(node->lhs->ty) || !eval_const(node->lhs, &a))
      return false;
    a = wrap_word(a, true) != 0;
    if ((node->kind == ND_LOGAND) ? !a : a) {
      *val = a;
      return true;
    }
    if (!is_scalar(node->rhs->ty) || !eval_const(node->rhs, &b))
      return false;
    *val = wrap_word(b, true) != 0;
    return true;
  case ND_COND:
    if (!is_scalar(node->cond->ty) || !eval_const(node->cond, &a))
      return false;
    return eval_const(wrap_word(a, true) ? node->then : node->els, val);
  default:
    return false;
  }

  *val = wrap_type(*val, node->ty);
  return true;
}

static bool is_const_int(Node *node, int64_t *val) {
  return eval_const(node, val);
}

// A word pointer variable, whose target can be reached through
// indirection ("@"), which is valid because addresses never have bit
// 35 set.
static bool is_ptr_var(Node *node) {
  node = strip_casts(node);
  return node->kind == ND_VAR && node->var->ty->kind == TY_PTR &&
         ptr_unit(node->var->ty->base) == WORD_CHARS;
}

static Obj *ptr_var(Node *node) {
  return strip_casts(node)->var;
}

// If the word-sized scalar lvalue `node` can be accessed with a single
// memory reference instruction, return the address field for it.
static char *operand(Node *node) {
  if (!is_word_scalar(node->ty))
    return NULL;

  switch (node->kind) {
  case ND_VAR:
    if (node->var->ty->kind == TY_VLA)
      return NULL;
    return var_operand(node->var);
  case ND_DEREF:
    if (is_ptr_var(node->lhs))
      return format("@%s", var_operand(ptr_var(node->lhs)));
    return NULL;
  case ND_MEMBER: {
    if (node->member->is_bitfield)
      return NULL;
    int off = node->member->offset / WORD_CHARS;
    Node *base = node->lhs;
    if (base->kind == ND_VAR && base->var->ty->kind != TY_VLA) {
      if (base->var->is_local)
        return disp(base->var->offset + off, "ap");
      if (off == 0 && !is_far(base->var))
        return sym(base->var->name);
      return NULL;
    }
    if (base->kind == ND_DEREF && is_ptr_var(base->lhs) && off == 0)
      return format("@%s", var_operand(ptr_var(base->lhs)));
    return NULL;
  }
  }
  return NULL;
}

// A scalar at a fixed place: in the frame, at a label, or at a constant
// offset from what a word pointer variable points to.
typedef struct {
  char *prep;  // instruction that loads the base register, or NULL
  char *word;  // address field of the word holding the value
  int pos;     // big-endian position of a char (0-3) or short (0-1)
  bool whole;  // a char or short with a word of its own
} Access;

// Find an lvalue's location as a variable or word pointer variable plus
// a constant offset in chars.
static bool static_lvalue(Node *node, Obj **var, Obj **ptr, int64_t *off) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->ty->kind == TY_VLA)
      return false;
    *var = node->var;
    *off = var_start(node->var->ty);
    return true;
  case ND_MEMBER:
    if (node->member->is_bitfield || !static_lvalue(node->lhs, var, ptr, off))
      return false;
    *off += node->member->offset;
    return true;
  case ND_DEREF: {
    Node *p = node->lhs;
    if (!is_pointer_like(p->ty))
      return false;
    int unit = value_unit(p->ty);
    int64_t c = 0;
    if ((p->kind == ND_ADD || p->kind == ND_SUB) && eval_const(p->rhs, &c)) {
      if (p->kind == ND_SUB)
        c = -c;
      p = p->lhs;
    }
    p = strip_casts(p);
    if (!is_pointer_like(p->ty) || value_unit(p->ty) != unit)
      return false;

    if (p->kind == ND_VAR && p->var->ty->kind == TY_ARRAY) {
      *var = p->var;
      *off = c * unit;
      return true;
    }
    if (is_ptr_var(p) && unit == WORD_CHARS) {
      *ptr = p->var;
      *off = c * unit;
      return true;
    }
    if (p->kind == ND_ADDR && static_lvalue(p->lhs, var, ptr, off)) {
      *off += c * unit;
      return true;
    }
    return false;
  }
  }
  return false;
}

static bool static_access(Node *node, Access *a) {
  if (!is_scalar(node->ty) || node->ty->size > WORD_CHARS)
    return false;

  Obj *var = NULL, *ptr = NULL;
  int64_t off;
  if (!static_lvalue(node, &var, &ptr, &off))
    return false;

  int64_t w = (off >= 0) ? off / WORD_CHARS : -((-off + WORD_CHARS - 1) / WORD_CHARS);
  int r = off - w * WORD_CHARS;
  int size = node->ty->size;
  if (r % size)
    return false;

  *a = (Access){NULL, NULL, r / size, false};
  a->whole = (node->kind == ND_VAR && var_start(node->ty) > 0);

  if (var && var->is_local) {
    a->word = disp(var->offset + w, "ap");
  } else if (var && is_far(var)) {
    a->prep = format("ld          x3, %s", far_word(var));
    a->word = disp(w, "x3");
  } else if (var && w == 0) {
    a->word = sym(var->name);
  } else if (var) {
    a->prep = format("ldea        x3, %s", sym(var->name));
    a->word = disp(w, "x3");
  } else if (w == 0) {
    a->word = format("@%s", var_operand(ptr));
  } else {
    a->prep = format("ld          x3, %s", var_operand(ptr));
    a->word = disp(w, "x3");
  }
  return true;
}

// Normalize a char or short in `reg` (taken from the low bits) to the
// way registers hold values of type `ty`.
static void extend(char *reg, Type *ty) {
  int bits = ty->size * CHAR_BITS;
  if (bits >= WORD_BITS)
    return;
  if (ty->is_unsigned) {
    emit("movmz", "%s, %s, %d", reg, reg, WORD_BITS - bits);
  } else {
    emit("movsz", "%s, %s, %s, %d", reg, reg, reg, WORD_BITS - bits);
    emit("movrtzn", "%s, %s, 1", reg, reg);
    emit("movsr", "%s, %s, %s, %d", reg, reg, reg, WORD_BITS - bits);
  }
}

// Extract a char or short at position `pos` of the word in `reg`.
static void extract(char *reg, Type *ty, int pos) {
  int bits = ty->size * CHAR_BITS;
  if (bits >= WORD_BITS)
    return;
  int rot = (bits * (pos + 1)) % WORD_BITS;  // brings it to the low end
  if (ty->is_unsigned) {
    if (rot)
      emit("movrz", "%s, %s, %d, %d", reg, reg, rot, WORD_BITS - bits);
    else
      emit("movmz", "%s, %s, %d", reg, reg, WORD_BITS - bits);
  } else {
    if (pos)
      emit("movr", "%s, %s, %d", reg, reg, bits * pos);  // to the high end
    emit("movrtzn", "%s, %s, 1", reg, reg);
    emit("movsr", "%s, %s, %s, %d", reg, reg, reg, WORD_BITS - bits);
  }
}

static void load_access(Access *a, Type *ty, char *reg) {
  if (a->prep)
    println("            %s", a->prep);
  emit("ld", "%s, %s", reg, a->word);
  extract(reg, ty, a->pos);
}

// Store X0 (preserved) through an Access. Clobbers X1.
static void store_access(Access *a, Type *ty) {
  if (a->prep)
    println("            %s", a->prep);
  int bits = ty->size * CHAR_BITS;
  if (bits >= WORD_BITS || a->whole) {
    emit("st", "x0, %s", a->word);
    return;
  }
  // A byte pointer: word address and the shift of the field.
  emit("ldea", "x1, %s", a->word);
  int shift = WORD_BITS - bits * (a->pos + 1);
  if (shift)
    emit("or", "x1, $$s%d", shift);
  emit("stb", "x0, x1, %d", bits);
}

// Convert the char or short pointer in `reg` to a hardware byte pointer
// (word address, plus the shift of the field in bits 27-35), using `tmp`.
static void to_byte_pointer(char *reg, int unit, char *tmp) {
  if (unit == 1) {
    emit("andi", "%s, %s, 3", reg, tmp);
    emit("movsrz", "%s, %s, %s, 2", reg, reg, reg);
    emit("add", "%s, $$bp(%s)", reg, tmp);
  } else {
    emit("andi", "%s, %s, 1", reg, tmp);
    emit("movsrz", "%s, %s, %s, 1", reg, reg, reg);
    emit("add", "%s, $$hp(%s)", reg, tmp);
  }
}

// Convert a pointer in X0 between representations (ptr_unit values).
static void convert_pointer(int from, int to) {
  int shift = log2_unit(to) - log2_unit(from);
  if (shift > 0)
    emit("movsz", "x0, x0, x0, %d", shift);
  else if (shift < 0)
    emit("movsrz", "x0, x0, x0, %d", -shift);
}

// X0 holds the word address of an object of type `ty` that starts
// `start` chars into the word; make it a pointer in ty's representation.
static void word_to_pointer(Type *ty, int start) {
  int unit = ptr_unit(ty);
  convert_pointer(WORD_CHARS, unit);
  if (start / unit)
    emit("addi", "x0, x0, %d", start / unit);
}

// Copy `n` words from the address in `src` to the address in `dst`.
// Clobbers X2-X5 but preserves `src` and `dst`.
static void copy_words(char *src, char *dst, int n) {
  if (n <= 6) {
    for (int i = 0; i < n; i++) {
      emit("ld", "x2, %d(%s)", i, src);
      emit("st", "x2, %d(%s)", i, dst);
    }
    return;
  }

  emit("movr", "%s, x3", src);
  emit("movr", "%s, x4", dst);
  load_imm("x2", n);
  emit("ld", "x5, 0(x3)");
  emit("st", "x5, 0(x4)");
  emit("incr", "x3, x3");
  emit("incr", "x4, x4");
  emit("addi.rn", "x2, x2, -1");
  emit("jmp", ".-5");
}

// Returns true if `node` can be evaluated straight into any register
// without disturbing X0 or the stack: a constant or a value at a fixed
// place. (Clobbers X3.)
static bool is_simple(Node *node) {
  int64_t val;
  Access a;
  if (eval_const(node, &val))
    return true;
  node = strip_casts(node);
  if (node->kind != ND_VAR && node->kind != ND_DEREF && node->kind != ND_MEMBER)
    return false;
  return operand(node) || static_access(node, &a);
}

static void gen_simple(Node *node, char *reg) {
  int64_t val;
  Access a;
  if (eval_const(node, &val)) {
    load_imm(reg, val);
    return;
  }
  Node *n = strip_casts(node);
  char *op = operand(n);
  if (op) {
    emit("ld", "%s, %s", reg, op);
  } else {
    static_access(n, &a);
    load_access(&a, n->ty, reg);
  }
}

// Compute the address of a given node into X0, in the representation
// for pointers to its type. It's an error if the node is not in memory.
static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    // Variable-length array, which is always local.
    if (node->var->ty->kind == TY_VLA) {
      emit("ld", "x0, %s", disp(node->var->offset, "ap"));
      return;
    }

    // Local variable
    if (node->var->is_local) {
      emit("ldea", "x0, %s", disp(node->var->offset, "ap"));
    } else if (is_far(node->var)) {
      emit("ld", "x0, %s", far_word(node->var));
    } else {
      // Function or global variable. (Thread-local variables are
      // ordinary globals; the ACR-7000 runs a single thread of C code.)
      emit("ldea", "x0, %s", sym(node->var->name));
    }
    word_to_pointer(node->var->ty, var_start(node->var->ty));
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  case ND_COMMA:
    gen_expr(node->lhs);
    gen_addr(node->rhs);
    return;
  case ND_MEMBER: {
    gen_addr(node->lhs);  // a struct or union: a word address
    int unit = ptr_unit(node->member->ty);
    int off = node->member->offset;
    convert_pointer(WORD_CHARS, unit);
    if (off / unit)
      emit("ldea", "x0, %s", disp(off / unit, "x0"));
    return;
  }
  case ND_FUNCALL:
    if (node->ret_buffer) {
      gen_expr(node);
      return;
    }
    break;
  case ND_ASSIGN:
  case ND_COND:
    if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION) {
      gen_expr(node);
      return;
    }
    break;
  case ND_VLA_PTR:
    emit("ldea", "x0, %s", disp(node->var->offset, "ap"));
    return;
  }

  error_tok(node->tok, "not an lvalue");
}

// Load a value of type `ty` from where X0 points. Clobbers X1.
static void load(Node *node, Type *ty) {
  switch (ty->kind) {
  case TY_ARRAY:
  case TY_STRUCT:
  case TY_UNION:
  case TY_FUNC:
  case TY_VLA:
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    no_float(node->tok);
  }

  if (ty->size >= WORD_CHARS) {
    emit("ld", "x0, 0(x0)");
    return;
  }

  to_byte_pointer("x0", ptr_unit(ty), "x1");
  emit("ldb", "x0, x0, %d", ty->size * CHAR_BITS);
  if (!ty->is_unsigned)
    extend("x0", ty);
}

// Store X0 to an address that the stack top is pointing to. Clobbers
// X1-X5.
static void store(Node *node, Type *ty) {
  pop("x1");

  switch (ty->kind) {
  case TY_STRUCT:
  case TY_UNION:
    copy_words("x0", "x1", words(ty->size));
    return;
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    no_float(node->tok);
  }

  if (ty->size >= WORD_CHARS) {
    emit("st", "x0, 0(x1)");
    return;
  }

  to_byte_pointer("x1", ptr_unit(ty), "x2");
  emit("stb", "x0, x1, %d", ty->size * CHAR_BITS);
}

static void cmp_zero_check(Node *node) {
  if (is_flonum(node->ty))
    no_float(node->tok);
}

static void cast(Node *node, Type *from, Type *to) {
  if (to->kind == TY_VOID)
    return;

  if (is_flonum(from) || is_flonum(to))
    no_float(node->tok);

  if (to->kind == TY_BOOL) {
    set_if_nonzero();
    return;
  }

  if (is_pointer_like(from) && is_pointer_like(to)) {
    convert_pointer(value_unit(from), value_unit(to));
    return;
  }

  if (is_integer(to) && !same_value(from, to)) {
    // Truncate to the width of a char or short.
    int bits = to->size * CHAR_BITS;
    if (to->is_unsigned) {
      emit("movmz", "x0, x0, %d", WORD_BITS - bits);
    } else {
      emit("movsz", "x0, x0, x0, %d", WORD_BITS - bits);
      emit("movrtzn", "x0, x0, 1");
      emit("movsr", "x0, x0, x0, %d", WORD_BITS - bits);
    }
  }
}

static void push_struct(Type *ty) {
  int n = words(ty->size);
  if (n == 0)
    return;
  emit("addea", "sp, %d", -n);
  depth += n;
  copy_words("x0", "sp", n);
}

static int push_args2(Node *args) {
  if (!args)
    return 0;
  int stack = push_args2(args->next);

  gen_expr(args);

  switch (args->ty->kind) {
  case TY_STRUCT:
  case TY_UNION:
    push_struct(args->ty);
    return stack + words(args->ty->size);
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    no_float(args->tok);
  default:
    push();
    return stack + 1;
  }
}

// Push function call arguments right to left, followed by the address
// of the return buffer if the function returns a struct or union.
// Every argument takes a whole number of words. Returns the number of
// words pushed.
static int push_args(Node *node) {
  int stack = push_args2(node->args);

  if (node->ret_buffer) {
    emit("ldea", "x0, %s", disp(node->ret_buffer->offset, "ap"));
    push();
    stack++;
  }
  return stack;
}

// alloca: X0 holds the size in chars. The temporaries between SP and
// the alloca bottom are moved down to make room, and X0 receives the
// address of the new block (a char pointer).
static void builtin_alloca(void) {
  char *bottom = disp(current_fn->alloca_bottom->offset, "ap");
  emit("addi", "x0, x0, %d", WORD_CHARS - 1);
  emit("movsrz", "x0, x0, x0, 2");  // size in words
  emit("ld", "x1, %s", bottom);
  emit("movr", "x1, x2");
  emit("subr", "sp, x2");       // x2 = number of temporary words
  emit("movr", "sp, x3");       // x3 = old SP
  emit("subr", "x0, sp");       // SP -= size
  emit("movr", "sp, x4");       // x4 = new SP
  emit("movr.rz", "x2, x2");
  emit("jmp", ".7");
  emit("ld", "x5, 0(x3)");
  emit("st", "x5, 0(x4)");
  emit("incr", "x3, x3");
  emit("incr", "x4, x4");
  emit("addi.rn", "x2, x2, -1");
  emit("jmp", ".-5");
  emit("ld", "x1, %s", bottom);
  emit("subr", "x0, x1");
  emit("st", "x1, %s", bottom);
  emit("movr", "x1, x0");
  convert_pointer(WORD_CHARS, 1);
}

// Load ("ld") or store ("st") `reg` through `ptr`, which holds a word
// address, or a byte pointer for chars and shorts. Loaded chars and
// shorts are zero-extended unless `sign` is set.
static void hw_access(char *op, char *reg, char *ptr, Type *ty, bool sign) {
  if (ty->size >= WORD_CHARS) {
    emit(op, "%s, 0(%s)", reg, ptr);
    return;
  }
  bool is_load = !strcmp(op, "ld");
  emit(is_load ? "ldb" : "stb", "%s, %s, %d", reg, ptr, ty->size * CHAR_BITS);
  if (is_load && sign && !ty->is_unsigned)
    extend(reg, ty);
}

// Extract a bit-field from its storage unit in X0.
static void load_bitfield(Member *mem) {
  shift_left(WORD_BITS - mem->bit_width - mem->bit_offset);
  shift_right(WORD_BITS - mem->bit_width, mem->ty->is_unsigned);
}

static void gen_binary_ops(Node *node);
static void gen_branch(Node *node, char *label, bool when);

// Evaluate `node` for its side effects only.
static void gen_effect(Node *node) {
  int64_t val;
  switch (node->kind) {
  case ND_NULL_EXPR:
  case ND_NUM:
  case ND_VAR:
    // No code is needed (not even for a floating-point value).
    return;
  case ND_CAST:
    if (is_flonum(node->ty) || is_flonum(node->lhs->ty))
      no_float(node->tok);
    gen_effect(node->lhs);
    return;
  case ND_COMMA:
    gen_effect(node->lhs);
    gen_effect(node->rhs);
    return;
  case ND_ADD:
  case ND_SUB:
    // e.g. the "- 1" that turns "x++" into an rvalue
    if (is_scalar(node->ty) && eval_const(node->rhs, &val)) {
      gen_effect(node->lhs);
      return;
    }
    break;
  }
  gen_expr(node);
}

// Generate code for a given node.
static void gen_expr(Node *node) {
  int64_t val;
  if (node->kind != ND_NUM && eval_const(node, &val)) {
    load_imm("x0", val);
    return;
  }

  if (node->kind == ND_VAR || node->kind == ND_DEREF || node->kind == ND_MEMBER) {
    char *op = operand(node);
    if (op) {
      emit("ld", "x0, %s", op);
      return;
    }
    Access a;
    if (static_access(node, &a)) {
      load_access(&a, node->ty, "x0");
      return;
    }
  }

  switch (node->kind) {
  case ND_NULL_EXPR:
    return;
  case ND_NUM:
    if (is_flonum(node->ty))
      no_float(node->tok);
    load_imm("x0", node->val);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    if (is_flonum(node->ty))
      no_float(node->tok);
    emit("ngtr", "x0, x0");
    return;
  case ND_VAR:
    gen_addr(node);
    load(node, node->ty);
    return;
  case ND_MEMBER: {
    gen_addr(node);
    load(node, node->ty);

    Member *mem = node->member;
    if (mem->is_bitfield)
      load_bitfield(mem);
    return;
  }
  case ND_DEREF:
    gen_expr(node->lhs);
    load(node, node->ty);
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_ASSIGN: {
    if (operand(node->lhs)) {
      gen_expr(node->rhs);
      if (is_flonum(node->ty))
        no_float(node->tok);
      emit("st", "x0, %s", operand(node->lhs));
      return;
    }

    Access a;
    if (static_access(node->lhs, &a)) {
      gen_expr(node->rhs);
      static_access(node->lhs, &a);
      store_access(&a, node->lhs->ty);
      return;
    }

    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);

    if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
      // If the lhs is a bitfield, we need to read the current value
      // from memory and merge it with a new value.
      Member *mem = node->lhs->member;
      uint64_t mask = ((mem->bit_width == WORD_BITS) ? WORD_MASK :
                       ((1ULL << mem->bit_width) - 1)) << mem->bit_offset;

      emit("movr", "x0, x3");       // the result of the expression
      load_imm("x2", ((1ULL << mem->bit_width) - 1) & WORD_MASK);
      emit("andr", "x2, x0");
      shift_left(mem->bit_offset);
      emit("movr", "x0, x4");       // new bits in place

      emit("ld", "x0, 0(sp)");      // the address
      load(node, mem->ty);
      load_imm("x2", ~mask & WORD_MASK);
      emit("andr", "x2, x0");
      emit("bisr", "x4, x0");
      store(node, node->ty);
      emit("movr", "x3, x0");
      return;
    }

    store(node, node->ty);
    return;
  }
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next) {
      if (!n->next && n->kind == ND_EXPR_STMT)
        gen_expr(n->lhs);
      else
        gen_stmt(n);
    }
    return;
  case ND_COMMA:
    gen_effect(node->lhs);
    gen_expr(node->rhs);
    return;
  case ND_CAST:
    gen_expr(node->lhs);
    cast(node, node->lhs->ty, node->ty);
    return;
  case ND_MEMZERO: {
    int n = words(node->var->ty->size);
    int off = node->var->offset;
    if (n <= 6) {
      emit("xorr", "x1, x1");
      for (int i = 0; i < n; i++)
        emit("st", "x1, %s", disp(off + i, "ap"));
      return;
    }
    emit("ldea", "x1, %s", disp(off, "ap"));
    load_imm("x2", n);
    emit("xorr", "x3, x3");
    emit("st", "x3, 0(x1)");
    emit("incr", "x1, x1");
    emit("addi.rn", "x2, x2, -1");
    emit("jmp", ".-3");
    return;
  }
  case ND_COND: {
    char *els = new_label();
    char *end = new_label();
    gen_branch(node->cond, els, false);
    gen_expr(node->then);
    emit("jmp", "%s", end);
    emit_label(els);
    gen_expr(node->els);
    emit_label(end);
    return;
  }
  case ND_NOT:
    gen_expr(node->lhs);
    cmp_zero_check(node->lhs);
    set_if_zero();
    return;
  case ND_BITNOT:
    gen_expr(node->lhs);
    emit("comr", "x0, x0");
    return;
  case ND_LOGAND:
  case ND_LOGOR: {
    char *f = new_label();
    gen_branch(node, f, false);
    emit("ldea", "x0, 1");
    emit("jmp", ".2");
    emit_label(f);
    emit("xorr", "x0, x0");
    return;
  }
  case ND_FUNCALL: {
    if (node->lhs->kind == ND_VAR && !strcmp(node->lhs->var->name, "alloca")) {
      gen_expr(node->args);
      builtin_alloca();
      return;
    }

    if (is_flonum(node->ty))
      no_float(node->tok);

    int stack_args = push_args(node);

    if (node->lhs->kind == ND_VAR && node->lhs->var->ty->kind == TY_FUNC) {
      emit("calls", "%s", sym(node->lhs->var->name));
    } else {
      gen_expr(node->lhs);
      emit("calls", "0(x0)");
    }

    if (stack_args)
      emit("addea", "sp, %d", stack_args);
    depth -= stack_args;

    // The return value (or the address of the returned struct) is in AC.
    if (node->ty->kind != TY_VOID)
      emit("movr", "ac, x0");
    return;
  }
  case ND_LABEL_VAL:
    // A void pointer, so a char pointer.
    emit("ldea", "x0, %s", sym(node->unique_label));
    convert_pointer(WORD_CHARS, 1);
    return;
  case ND_CAS: {
    Type *ty = node->cas_addr->ty->base;
    gen_expr(node->cas_addr);
    push();
    gen_expr(node->cas_new);
    push();
    gen_expr(node->cas_old);
    emit("movr", "x0, x3");           // x3 = pointer to the expected value
    pop("xy");                        // xy = new value
    pop("x1");                        // x1 = address
    if (ty->size < WORD_CHARS) {
      to_byte_pointer("x1", ptr_unit(ty), "x2");
      to_byte_pointer("x3", ptr_unit(ty), "x2");
    }

    // Interrupts are never enabled while C code runs, so a plain
    // compare-and-store is atomic.
    hw_access("ld", "x2", "x3", ty, false);  // x2 = expected value
    hw_access("ld", "mq", "x1", ty, false);  // mq = current value
    emit("cmpne", "mq, x2");
    emit("jmp", ".4");
    hw_access("st", "xy", "x1", ty, false);
    emit("ldea", "x0, 1");
    emit("jmp", ".3");
    hw_access("st", "mq", "x3", ty, false);
    emit("xorr", "x0, x0");
    return;
  }
  case ND_EXCH: {
    Type *ty = node->lhs->ty->base;
    gen_expr(node->lhs);
    push();
    gen_expr(node->rhs);
    pop("x1");
    if (ty->size < WORD_CHARS)
      to_byte_pointer("x1", ptr_unit(ty), "x2");
    hw_access("ld", "x2", "x1", ty, true);
    hw_access("st", "x0", "x1", ty, false);
    emit("movr", "x2, x0");
    return;
  }
  }

  if (is_flonum(node->lhs->ty) || is_flonum(node->rhs->ty))
    no_float(node->tok);

  gen_binary_ops(node);
}

static bool is_comparison(Node *node) {
  return (node->kind == ND_EQ || node->kind == ND_NE || node->kind == ND_LT ||
          node->kind == ND_LE) &&
         is_scalar(node->lhs->ty) && is_scalar(node->rhs->ty);
}

// Load the operands of a binary operator: X0 = lhs, X1 = rhs.
static void gen_operands(Node *node) {
  if (is_simple(node->rhs)) {
    gen_expr(node->lhs);
    gen_simple(node->rhs, "x1");
  } else if (is_simple(node->lhs)) {
    gen_expr(node->rhs);
    emit("movr", "x0, x1");
    gen_simple(node->lhs, "x0");
  } else {
    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("x1");
  }
}

// Jump to `label` if the truth value of `node` equals `when`.
static void gen_branch(Node *node, char *label, bool when) {
  int64_t val;
  if (is_scalar(node->ty) && eval_const(node, &val)) {
    if ((val != 0) == when)
      emit("jmp", "%s", label);
    return;
  }

  switch (node->kind) {
  case ND_NOT:
    if (is_scalar(node->lhs->ty)) {
      gen_branch(node->lhs, label, !when);
      return;
    }
    break;
  case ND_CAST:
    if (is_scalar(node->ty) && is_scalar(node->lhs->ty)) {
      gen_branch(node->lhs, label, when);
      return;
    }
    break;
  case ND_LOGAND:
  case ND_LOGOR:
    if ((node->kind == ND_LOGAND) == when) {
      // a && b: jump if both are true; a || b: jump if both are false
      char *skip = new_label();
      gen_branch(node->lhs, skip, !when);
      gen_branch(node->rhs, label, when);
      emit_label(skip);
    } else {
      // a && b: jump if either is false; a || b: jump if either is true
      gen_branch(node->lhs, label, when);
      gen_branch(node->rhs, label, when);
    }
    return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE: {
    if (!is_comparison(node))
      break;

    if (node->kind == ND_EQ || node->kind == ND_NE) {
      bool jump_if_equal = (node->kind == ND_EQ) == when;
      int64_t c;
      if (eval_const(node->rhs, &c) && fits_imm13(-wrap_word(c, false))) {
        // The next instruction runs if the sum is zero (.rz) or not (.rn).
        gen_expr(node->lhs);
        emit(jump_if_equal ? "addin.rz" : "addin.rn", "x0, x0, %ld",
             (long)-wrap_word(c, false));
      } else {
        gen_operands(node);
        emit(jump_if_equal ? "cmp" : "cmpne", "x1, x0");
      }
      emit("jmp", "%s", label);
      return;
    }

    // Signed comparisons with zero test the sign bit: rotating it to
    // bit 0 and masking off the rest leaves 0 or 1, and the next
    // instruction runs if that is nonzero (.rn) or zero (.rz).
    int64_t zero;
    if (!node->lhs->ty->is_unsigned) {
      if (node->kind == ND_LT && eval_const(node->rhs, &zero) && zero == 0) {
        gen_expr(node->lhs);  // x < 0
        emit(when ? "movrzn.rn" : "movrzn.rz", "x0, x0, 1, 35");
        emit("jmp", "%s", label);
        return;
      }
      if (node->kind == ND_LE && eval_const(node->lhs, &zero) && zero == 0) {
        gen_expr(node->rhs);  // 0 <= x
        emit(when ? "movrzn.rz" : "movrzn.rn", "x0, x0, 1, 35");
        emit("jmp", "%s", label);
        return;
      }
    }

    gen_operands(node);
    if (!node->lhs->ty->is_unsigned) {
      emit("xor", "x0, $$sgn");
      emit("xor", "x1, $$sgn");
    }
    // The carry receives the result of the comparison; the next
    // instruction runs if it is set (.cn) or clear (.cz).
    char *test = when ? "cn" : "cz";
    if (node->kind == ND_LT)
      emit(format("subrsn.%s", test), "x1, x0");  // carry = x0 < x1
    else
      emit(format("subrzn.%s", test), "x0, x1");  // carry = x0 <= x1
    emit("jmp", "%s", label);
    return;
  }
  }

  gen_expr(node);
  cmp_zero_check(node);
  if (when)
    jump_if_nonzero(label);
  else
    jump_if_zero(label);
}

static int log2_exact(int64_t v) {
  if (v <= 0 || (v & (v - 1)))
    return -1;
  int n = 0;
  while ((1LL << n) != v)
    n++;
  return n;
}

// Binary operators. X0 = lhs, X1 = rhs.
static void gen_binary_ops(Node *node) {
  int64_t c;

  // Put a constant operand of a commutative operator on the right.
  switch (node->kind) {
  case ND_ADD:
  case ND_MUL:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
  case ND_EQ:
  case ND_NE:
    if ((is_const_int(node->lhs, &c) && !is_const_int(node->rhs, &c)) ||
        (is_simple(node->lhs) && !is_simple(node->rhs))) {
      Node *tmp = node->lhs;
      node->lhs = node->rhs;
      node->rhs = tmp;
    }
  }

  bool rhs_const = is_const_int(node->rhs, &c);
  if (rhs_const)
    c = wrap_word(c, false);

  // Multiplication and division by simple constants.
  if (rhs_const && node->kind == ND_MUL) {
    int n = log2_exact(c);
    if (c == 0) {
      gen_effect(node->lhs);
      emit("xorr", "x0, x0");
      return;
    }
    if (n >= 0) {
      gen_expr(node->lhs);
      shift_left(n);
      return;
    }
  }

  if (rhs_const && (node->kind == ND_DIV || node->kind == ND_MOD)) {
    int n = log2_exact(c);
    if (c == 1) {
      gen_expr(node->lhs);
      if (node->kind == ND_MOD)
        emit("xorr", "x0, x0");
      return;
    }
    if (n >= 0 && node->ty->is_unsigned) {
      gen_expr(node->lhs);
      if (node->kind == ND_DIV)
        shift_right(n, true);
      else if (fits_imm13(c - 1))
        emit("andi", "x0, x0, %ld", (long)(c - 1));
      else
        emit("movmz", "x0, x0, %d", WORD_BITS - n);
      return;
    }
  }

  // Shifts by a constant amount. (Out-of-range counts are undefined
  // behavior; they shift everything out.)
  if (rhs_const && (node->kind == ND_SHL || node->kind == ND_SHR)) {
    gen_expr(node->lhs);
    bool is_unsigned = node->lhs->ty->is_unsigned;
    int n = ((uint64_t)c >= WORD_BITS) ? WORD_BITS : c;

    if (node->kind == ND_SHL && n == WORD_BITS)
      emit("xorr", "x0, x0");
    else if (node->kind == ND_SHL)
      shift_left(n);
    else if (n == WORD_BITS && is_unsigned)
      emit("xorr", "x0, x0");
    else
      shift_right(MIN(n, WORD_BITS - 1), is_unsigned);
    return;
  }

  // Operations with a small constant operand use immediate forms.
  if (rhs_const && fits_imm13(c)) {
    switch (node->kind) {
    case ND_ADD:
      gen_expr(node->lhs);
      if (c)
        emit("addi", "x0, x0, %ld", (long)c);
      return;
    case ND_SUB:
      if (fits_imm13(-c)) {
        gen_expr(node->lhs);
        if (c)
          emit("addi", "x0, x0, %ld", (long)-c);
        return;
      }
      break;
    case ND_BITAND:
      gen_expr(node->lhs);
      emit("andi", "x0, x0, %ld", (long)c);
      return;
    case ND_BITOR:
      gen_expr(node->lhs);
      emit("bisi", "x0, x0, %ld", (long)c);
      return;
    case ND_BITXOR:
      gen_expr(node->lhs);
      emit("xori", "x0, x0, %ld", (long)c);
      return;
    case ND_EQ:
      gen_expr(node->lhs);
      if (c)
        emit("addi", "x0, x0, %ld", (long)-c);
      set_if_zero();
      return;
    case ND_NE:
      gen_expr(node->lhs);
      if (c)
        emit("addi", "x0, x0, %ld", (long)-c);
      set_if_nonzero();
      return;
    }
  }

  // Operators with a memory operand form.
  char *mem = NULL;
  if (!rhs_const && is_simple(node->rhs))
    mem = operand(strip_casts(node->rhs));

  if (mem) {
    char *insn = NULL;
    switch (node->kind) {
    case ND_ADD: insn = "add"; break;
    case ND_SUB: insn = "sub"; break;
    case ND_BITAND: insn = "and"; break;
    case ND_BITOR: insn = "or"; break;
    case ND_BITXOR: insn = "xor"; break;
    }

    if (insn) {
      gen_expr(node->lhs);
      emit(insn, "x0, %s", mem);
      return;
    }

    if (node->kind == ND_MUL) {
      if (is_simple(node->lhs)) {
        gen_simple(node->lhs, "mq");
      } else {
        gen_expr(node->lhs);
        emit("movr", "x0, mq");
      }
      emit("mul", "%s", mem);
      emit("movr", "ac, x0");
      return;
    }

    if (node->kind == ND_DIV || node->kind == ND_MOD) {
      if (is_simple(node->lhs)) {
        gen_simple(node->lhs, "ac");
      } else {
        gen_expr(node->lhs);
        emit("movr", "x0, ac");
      }
      emit(node->ty->is_unsigned ? "udiv" : "div", "%s", mem);
      emit("movr", "%s, x0", node->kind == ND_DIV ? "mq" : "xy");
      return;
    }
  }

  gen_operands(node);

  bool is_unsigned = node->lhs->ty->is_unsigned;

  switch (node->kind) {
  case ND_ADD:
    emit("addr", "x1, x0");
    return;
  case ND_SUB:
    emit("subr", "x1, x0");
    return;
  case ND_MUL:
    emit("movr", "x0, mq");
    emit("push", "x1");
    emit("mul", "+1");
    emit("movr", "ac, x0");
    return;
  case ND_DIV:
  case ND_MOD:
    emit("movr", "x0, ac");
    emit("push", "x1");
    emit(node->ty->is_unsigned ? "udiv" : "div", "+1");
    emit("movr", "%s, x0", node->kind == ND_DIV ? "mq" : "xy");
    return;
  case ND_BITAND:
    emit("andr", "x1, x0");
    return;
  case ND_BITOR:
    emit("bisr", "x1, x0");
    return;
  case ND_BITXOR:
    emit("xorr", "x1, x0");
    return;
  case ND_EQ:
    emit("xorr", "x1, x0");
    set_if_zero();
    return;
  case ND_NE:
    emit("xorr", "x1, x0");
    set_if_nonzero();
    return;
  case ND_LT:
  case ND_LE:
    // Signed comparisons flip the sign bits and compare unsigned.
    if (!is_unsigned) {
      emit("xor", "x0, $$sgn");
      emit("xor", "x1, $$sgn");
    }
    if (node->kind == ND_LT)
      emit("subrsn", "x1, x0");      // carry = x0 < x1
    else
      emit("subrzn", "x0, x1");      // carry = x0 <= x1
    carry_to_x0();
    return;
  case ND_SHL:
    emit("andi", "x1, x1, 63");
    emit("edit", "x1, $$shl");
    return;
  case ND_SHR:
    emit("andi", "x1, x1, 63");
    if (is_unsigned) {
      emit("edit", "x1, $$shr");
    } else {
      emit("movrtzn", "x0, x0, 1");
      emit("edit", "x1, $$sar");
    }
    return;
  }

  error_tok(node->tok, "invalid expression");
}

static void gen_stmt(Node *node) {
  switch (node->kind) {
  case ND_IF: {
    char *els = new_label();
    gen_branch(node->cond, els, false);
    gen_stmt(node->then);
    if (node->els) {
      char *end = new_label();
      emit("jmp", "%s", end);
      emit_label(els);
      gen_stmt(node->els);
      emit_label(end);
    } else {
      emit_label(els);
    }
    return;
  }
  case ND_FOR: {
    char *begin = new_label();
    hashmap_put(&prunable, label_of(node->brk_label), "");
    hashmap_put(&prunable, label_of(node->cont_label), "");
    if (node->init)
      gen_stmt(node->init);
    emit_label(begin);
    if (node->cond)
      gen_branch(node->cond, label_of(node->brk_label), false);
    gen_stmt(node->then);
    emit_label(label_of(node->cont_label));
    if (node->inc)
      gen_effect(node->inc);
    emit("jmp", "%s", begin);
    emit_label(label_of(node->brk_label));
    return;
  }
  case ND_DO: {
    char *begin = new_label();
    hashmap_put(&prunable, label_of(node->brk_label), "");
    hashmap_put(&prunable, label_of(node->cont_label), "");
    emit_label(begin);
    gen_stmt(node->then);
    emit_label(label_of(node->cont_label));
    gen_branch(node->cond, begin, true);
    emit_label(label_of(node->brk_label));
    return;
  }
  case ND_SWITCH:
    hashmap_put(&prunable, label_of(node->brk_label), "");
    gen_expr(node->cond);

    for (Node *n = node->case_next; n; n = n->case_next) {
      int64_t begin = wrap_word(n->begin, false);
      int64_t end = wrap_word(n->end, false);

      if (begin == end) {
        if (fits_imm13(-begin)) {
          emit("addin.rz", "x0, x0, %ld", (long)-begin);
        } else {
          load_imm("x1", begin);
          emit("cmp", "x1, x0");
        }
        emit("jmp", "%s", label_of(n->label));
        continue;
      }

      // [GNU] Case ranges
      load_imm("x1", begin);
      emit("movr", "x0, x2");
      emit("subr", "x1, x2");
      load_imm("x1", end - begin);
      emit("cmple", "x2, x1");
      emit("jmp", "%s", label_of(n->label));
    }

    if (node->default_case)
      emit("jmp", "%s", label_of(node->default_case->label));

    emit("jmp", "%s", label_of(node->brk_label));
    gen_stmt(node->then);
    emit_label(label_of(node->brk_label));
    return;
  case ND_CASE:
    emit_label(label_of(node->label));
    gen_stmt(node->lhs);
    return;
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_GOTO:
    emit("jmp", "%s", label_of(node->unique_label));
    return;
  case ND_GOTO_EXPR:
    gen_expr(node->lhs);  // a void pointer
    convert_pointer(1, WORD_CHARS);
    emit("jmp", "0(x0)");
    return;
  case ND_LABEL:
    emit_label(label_of(node->unique_label));
    gen_stmt(node->lhs);
    return;
  case ND_RETURN:
    if (node->lhs) {
      Type *ty = node->lhs->ty;
      if (is_simple(node->lhs) && is_scalar(ty))
        gen_simple(node->lhs, "ac");
      else
        gen_expr(node->lhs);

      switch (ty->kind) {
      case TY_STRUCT:
      case TY_UNION:
        // Copy the value to the caller's buffer and return its address.
        emit("ld", "x1, %s", disp(current_fn->params->offset, "ap"));
        copy_words("x0", "x1", words(ty->size));
        emit("movr", "x1, x0");
        break;
      case TY_FLOAT:
      case TY_DOUBLE:
      case TY_LDOUBLE:
        no_float(node->lhs->tok);
      }
      if (is_simple(node->lhs) && ty->kind != TY_STRUCT && ty->kind != TY_UNION)
        ; // already in AC
      else
        emit("movr", "x0, ac");
    }

    if (node != tail_return)
      emit("jmp", "%s", ret_label);
    return;
  case ND_EXPR_STMT:
    gen_effect(node->lhs);
    return;
  case ND_ASM:
    println("%s", node->asm_str);
    return;
  }

  error_tok(node->tok, "invalid statement");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function)
      continue;

    // Parameters (including the hidden return buffer pointer, which is
    // first) were pushed by the caller above the call frame. Offsets
    // are in words: every variable starts a word of its own.
    int top = 3;
    for (Obj *var = fn->params; var; var = var->next) {
      var->offset = top;
      top += words(var->ty->size);
    }

    int bottom = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      if (var->offset > 0)
        continue;
      if (var == fn->alloca_bottom && !fn->uses_alloca)
        continue;
      bottom += words(var->ty->size);
      var->offset = -bottom;
    }

    fn->stack_size = bottom;
  }
}

//
// Data
//

static void emit_words(uint64_t *data, int len) {
  for (int i = 0; i < len;) {
    int n = MIN(8, len - i);
    fprintf(output_file, "            dw          ");
    for (int j = 0; j < n; j++) {
      uint64_t w = data[i + j] & WORD_MASK;
      if (w)
        fprintf(output_file, "%s0%lo", j ? ", " : "", (unsigned long)w);
      else
        fprintf(output_file, "%s0", j ? ", " : "");
    }
    fprintf(output_file, "\n");
    i += n;
  }
}

// Reserve `n` words that crt0 clears at startup.
static void emit_zeros(char *label, int n) {
  if (n <= 0)
    return;
  if (!label) {
    label = new_label();
    emit_label(label);
  }
  emit("bss", "%d", n);
  println(";@zero %s %d", label, n);
}

static void emit_var(Obj *var, char *label) {
  bool is_global = !var->is_static;
  int n = words(var->ty->size);

  // Common symbol: the linker places one definition.
  if (opt_fcommon && var->is_tentative && is_global) {
    println(";@comm %s %d %s", label, n, var->name);
    hashmap_put(&defined, label, label);
    return;
  }

  if (is_global) {
    println(";@def %s %s", label, var->name);
    hashmap_put(&defined, label, label);
  }

  // Large zero-filled objects go at the end of the program (see
  // acrlink.c) to keep code and other data within reach of 18-bit
  // addresses.
  if (!var->init_data && n >= BIG_BSS) {
    println(";@bigbss %s %d", label, n);
    return;
  }

  emit_label(label);
  if (!var->init_data) {
    emit_zeros(label, n);
    return;
  }

  // Pack the image's chars into words. A char or short on its own
  // occupies the low end of its word.
  uint64_t *data = calloc(n, sizeof(uint64_t));
  int start = var_start(var->ty);
  for (int i = 0; i < var->ty->size; i++) {
    int c = start + i;
    data[c / WORD_CHARS] |= var->init_data[i] <<
                            (CHAR_BITS * (WORD_CHARS - 1 - c % WORD_CHARS));
  }

  Relocation *rel = var->rel;
  int pos = 0;
  while (pos < n) {
    if (rel && rel->offset / WORD_CHARS == pos) {
      char *target = sym(*rel->label);
      int shift = log2_unit(rel->unit);
      if (rel->addend || shift) {
        // asm2 has no address arithmetic; crt0 patches in the value.
        char *word = new_label();
        emit_label(word);
        println(";@fixup %s %s %ld %d", word, target, (long)rel->addend, shift);
      }
      emit("dw", "%s", target);
      rel = rel->next;
      pos++;
      continue;
    }

    // Run of plain words up to the next relocation.
    int end = rel ? rel->offset / WORD_CHARS : n;
    int first = pos;
    while (pos < end) {
      // Long runs of zeros are left to crt0.
      int z = pos;
      while (z < end && data[z] == 0)
        z++;
      if (z - pos >= 16) {
        emit_words(data + first, pos - first);
        emit_zeros(NULL, z - pos);
        first = pos = z;
        continue;
      }
      pos = (z > pos) ? z : pos + 1;
    }
    emit_words(data + first, pos - first);
  }
}

// Emit global data. Anonymous objects (string literals, compound
// literals, static locals) are only emitted once something refers to
// them; returns true if anything was emitted.
static HashMap emitted;

static bool emit_data(Obj *prog) {
  bool progress = false;
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function || !var->is_definition)
      continue;
    if (hashmap_get(&emitted, var->name))
      continue;
    if (is_internal(var->name) && !hashmap_get(&used, label_of(var->name)))
      continue;
    hashmap_put(&emitted, var->name, var);
    progress = true;

    char *label = label_of(var->name);
    begin_section(label);
    emit_var(var, label);
    end_section();
  }
  return progress;
}

static bool is_word_char(char c) {
  return isalnum((unsigned char)c) || c == '_' || c == '$' || c == '?' || c == '.';
}

// Remove definitions of compiler-generated labels that nothing refers to
// (asm2 can only hold 4096 labels).
static void prune_labels(char *text, size_t len, FILE *out) {
  HashMap refs = {};
  for (char *p = text; p < text + len;) {
    char *eol = memchr(p, '\n', text + len - p);
    if (!eol)
      eol = text + len;
    bool is_def = (*p == '$');
    char *q = p;
    if (is_def)
      while (q < eol && *q != ':')
        q++;
    for (char *w = q; w < eol;) {
      if (*w == ';')
        break;
      if (*w == '$' && (w == q || !is_word_char(w[-1]) || w[-1] == '.')) {
        char *e = w;
        while (e < eol && is_word_char(*e))
          e++;
        hashmap_put2(&refs, w, e - w, (void *)1);
        w = e;
      } else {
        w++;
      }
    }
    p = eol + 1;
  }

  for (char *p = text; p < text + len;) {
    char *eol = memchr(p, '\n', text + len - p);
    if (!eol)
      eol = text + len;
    if (*p == '$' && eol[-1] == ':' && !hashmap_get2(&refs, p, eol - 1 - p) &&
        hashmap_get2(&prunable, p, eol - 1 - p)) {
      p = eol + 1;
      continue;
    }
    fwrite(p, 1, MIN(eol + 1, text + len) - p, out);
    p = eol + 1;
  }
}

static void emit_text(Obj *prog) {
  FILE *final_out = output_file;
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition)
      continue;

    // No code is emitted for "static inline" functions
    // if no one is referencing them.
    if (!fn->is_live)
      continue;

    char *label = label_of(fn->name);
    char *buf;
    size_t buflen;
    output_file = open_memstream(&buf, &buflen);

    fprintf(output_file, "\n");
    begin_section(label);
    println("; %s", fn->name);
    if (!fn->is_static) {
      println(";@def %s %s", label, fn->name);
      hashmap_put(&defined, label, label);
    }

    current_fn = fn;
    ret_label = new_label();

    tail_return = NULL;
    if (fn->body->kind == ND_BLOCK && fn->body->body) {
      Node *last = fn->body->body;
      while (last->next)
        last = last->next;
      if (last->kind == ND_RETURN)
        tail_return = last;
    }

    // Prologue. The label addresses the register save mask for calls.
    emit_label(label);
    emit("save", "ap");
    emit("movr", "sp, ap");
    if (fn->stack_size)
      emit("addea", "sp, %d", -fn->stack_size);
    if (fn->uses_alloca)
      emit("st", "sp, %s", disp(fn->alloca_bottom->offset, "ap"));

    // Emit code
    gen_stmt(fn->body);
    assert(depth == 0);

    // [https://www.sigbus.info/n1570#5.1.2.2.3p1] The C spec defines
    // a special rule for the main function. Reaching the end of the
    // main function is equivalent to returning 0, even though the
    // behavior is undefined for the other functions.
    if (strcmp(fn->name, "main") == 0 && !tail_return)
      emit("xorr", "ac, ac");

    // Epilogue
    emit_label(ret_label);
    emit("movr", "ap, sp");
    emit0("rets");
    end_section();

    fclose(output_file);
    output_file = final_out;
    prune_labels(buf, buflen, output_file);
    free(buf);
  }
}

void codegen(Obj *prog, FILE *out) {
  output_file = out;
  unit = acr_unit_id(base_file);

  println("; ACR-7000 assembly generated by chibicc from %s", base_file);
  println(";@unit %s", unit);

  assign_labels(prog);
  assign_lvar_offsets(prog);

  // Generate code first so we know which anonymous data is needed, but
  // place data first so that most references to it are backward ones
  // (asm2 can only hold 4096 unresolved forward references).
  char *text;
  size_t textlen;
  output_file = open_memstream(&text, &textlen);
  emit_text(prog);
  fclose(output_file);

  char *data;
  size_t datalen;
  output_file = open_memstream(&data, &datalen);
  while (emit_data(prog))
    ;
  for (int i = 0; i < far_list.len; i++) {
    char *word = hashmap_get(&far_words, far_list.data[i]);
    begin_section(word);
    emit_label(word);
    emit("dw", "%s", far_list.data[i]);
    hashmap_put(&sect_uses, far_list.data[i], "");
    strarray_push(&sect_list, far_list.data[i]);
    end_section();
  }
  fclose(output_file);

  output_file = out;
  fwrite(data, datalen, 1, out);
  fwrite(text, textlen, 1, out);

  fprintf(output_file, "\n");
  for (int i = 0; i < refs.len; i++)
    if (!hashmap_get(&defined, refs.data[i]))
      println(";@ref %s %s", refs.data[i],
              (char *)hashmap_get(&cnames, refs.data[i]));
}
