// A small C library for chibicc on the ACR-7000.
//
// Every integer type is one 36-bit word and chars are words too, so
// strings hold one character per word. Output goes to the line printer
// (device 013) and input comes from the paper tape reader (device 012).
// Code here runs in supervisor state at IRQL 0 with interrupts never
// taken, so devices are polled.

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int errno;

//
// Devices
//

struct __FILE {
  int fd;
  int unget;
};

static FILE files[3] = {{0, EOF}, {1, EOF}, {2, EOF}};
FILE *stdin = &files[0];
FILE *stdout = &files[1];
FILE *stderr = &files[2];

static int column;

// The line printer prints a line once it sees CR, LF or FF (or 132
// characters), so make sure a trailing partial line gets printed.
static void flush_printer(void) {
  if (column)
    putchar('\n');
}

int putchar(int c) {
  asm("            ld          x0, 3(ap)\n"
      "            wios        x0, 0, #00b\n"
      "            tiobz       #00b\n"
      "            jmp         .-1");
  c &= 0377;
  column = (c == '\n' || c == '\r' || c == '\f') ? 0 : column + 1;
  if (column == 132)
    column = 0;
  return c;
}

// Read a character from the paper tape reader. (The value is left in AC,
// the return value register.)
static int read_tape_raw(void) {
  asm("            nios        #00a\n"
      "            tiond       #00a\n"
      "            jmp         .-1\n"
      "            rio         ac, 0, #00a");
}

// The reader returns the code 0777 at the end of the input.
static int read_tape(void) {
  int c = read_tape_raw();
  return c == 0777 ? EOF : c;
}

int fputc(int c, FILE *fp) {
  if (fp->fd == 0)
    return EOF;
  return putchar(c);
}

int putc(int c, FILE *fp) {
  return fputc(c, fp);
}

int fputs(const char *s, FILE *fp) {
  while (*s)
    fputc(*s++, fp);
  return 0;
}

int puts(const char *s) {
  fputs(s, stdout);
  putchar('\n');
  return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *fp) {
  const char *p = ptr;
  for (size_t i = 0; i < size * n; i++)
    fputc(p[i], fp);
  return n;
}

int fflush(FILE *fp) {
  return 0;
}

int fgetc(FILE *fp) {
  if (fp->fd != 0)
    return EOF;
  if (fp->unget != EOF) {
    int c = fp->unget;
    fp->unget = EOF;
    return c;
  }
  return read_tape();
}

int getc(FILE *fp) {
  return fgetc(fp);
}

int getchar(void) {
  return fgetc(stdin);
}

int ungetc(int c, FILE *fp) {
  if (c == EOF)
    return EOF;
  fp->unget = c;
  return c;
}

char *fgets(char *buf, int n, FILE *fp) {
  int i = 0;
  while (i < n - 1) {
    int c = fgetc(fp);
    if (c == EOF)
      break;
    buf[i++] = c;
    if (c == '\n')
      break;
  }
  if (i == 0)
    return NULL;
  buf[i] = '\0';
  return buf;
}

//
// Formatted output
//

typedef struct {
  FILE *fp;     // stream, or NULL for a buffer
  char *buf;
  size_t cap;   // buffer size including the terminator
  size_t len;   // characters produced so far
} Out;

static void out_char(Out *o, int c) {
  if (o->fp)
    fputc(c, o->fp);
  else if (o->len + 1 < o->cap)
    o->buf[o->len] = c;
  o->len++;
}

static void out_pad(Out *o, int c, int n) {
  while (n-- > 0)
    out_char(o, c);
}

static int do_printf(Out *o, const char *fmt, va_list ap) {
  for (const char *p = fmt; *p; p++) {
    if (*p != '%') {
      out_char(o, *p);
      continue;
    }

    // Flags
    int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
    for (;; p++) {
      if (p[1] == '-') left = 1;
      else if (p[1] == '0') zero = 1;
      else if (p[1] == '+') plus = 1;
      else if (p[1] == ' ') space = 1;
      else if (p[1] == '#') alt = 1;
      else break;
    }
    p++;

    // Width and precision
    int width = 0;
    if (*p == '*') {
      width = va_arg(ap, int);
      if (width < 0) {
        left = 1;
        width = -width;
      }
      p++;
    } else {
      while (isdigit(*p))
        width = width * 10 + (*p++ - '0');
    }

    int prec = -1;
    if (*p == '.') {
      p++;
      prec = 0;
      if (*p == '*') {
        prec = va_arg(ap, int);
        p++;
      } else {
        while (isdigit(*p))
          prec = prec * 10 + (*p++ - '0');
      }
    }

    // Length modifiers are irrelevant: every integer type is one word.
    while (*p == 'h' || *p == 'l' || *p == 'L' || *p == 'q' ||
           *p == 'j' || *p == 'z' || *p == 't')
      p++;

    char tmp[40];
    char *digits = tmp + sizeof(tmp);
    int ndigits = 0;
    char *prefix = "";
    char *str;
    int len;

    switch (*p) {
    case 'd':
    case 'i':
    case 'u':
    case 'o':
    case 'x':
    case 'X':
    case 'p': {
      unsigned long v;
      int base = 10;
      char *xdigits = "0123456789abcdef";

      if (*p == 'd' || *p == 'i') {
        long n = va_arg(ap, long);
        if (n < 0) {
          prefix = "-";
          v = -(unsigned long)n;
        } else {
          prefix = plus ? "+" : space ? " " : "";
          v = n;
        }
      } else {
        v = va_arg(ap, unsigned long);
        if (*p == 'o') {
          base = 8;
        } else if (*p == 'x' || *p == 'p') {
          base = 16;
          if ((alt && v) || *p == 'p')
            prefix = "0x";
        } else if (*p == 'X') {
          base = 16;
          xdigits = "0123456789ABCDEF";
          if (alt && v)
            prefix = "0X";
        }
      }

      while (v) {
        *--digits = xdigits[v % base];
        v /= base;
        ndigits++;
      }
      if (alt && *p == 'o' && (prec <= ndigits)) {
        *--digits = '0';
        ndigits++;
      }
      if (prec < 0 && ndigits == 0) {
        *--digits = '0';
        ndigits++;
      }

      int zeros = (prec > ndigits) ? prec - ndigits : 0;
      int total = strlen(prefix) + zeros + ndigits;
      if (zero && !left && prec < 0 && width > total) {
        zeros += width - total;
        total = width;
      }

      if (!left)
        out_pad(o, ' ', width - total);
      for (char *q = prefix; *q; q++)
        out_char(o, *q);
      out_pad(o, '0', zeros);
      for (int i = 0; i < ndigits; i++)
        out_char(o, digits[i]);
      if (left)
        out_pad(o, ' ', width - total);
      continue;
    }
    case 'c':
      tmp[0] = va_arg(ap, int);
      str = tmp;
      len = 1;
      break;
    case 's':
      str = va_arg(ap, char *);
      if (!str)
        str = "(null)";
      len = (prec >= 0) ? strnlen(str, prec) : strlen(str);
      break;
    case 'n':
      *va_arg(ap, int *) = o->len;
      continue;
    case '%':
      out_char(o, '%');
      continue;
    default:
      // Unknown or floating-point conversion: print it literally.
      out_char(o, '%');
      if (!*p)
        return o->len;
      out_char(o, *p);
      continue;
    }

    if (!left)
      out_pad(o, ' ', width - len);
    for (int i = 0; i < len; i++)
      out_char(o, str[i]);
    if (left)
      out_pad(o, ' ', width - len);
  }

  return o->len;
}

int vfprintf(FILE *fp, const char *fmt, va_list ap) {
  Out o = {fp};
  return do_printf(&o, fmt, ap);
}

int vprintf(const char *fmt, va_list ap) {
  return vfprintf(stdout, fmt, ap);
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
  Out o = {NULL, buf, n};
  do_printf(&o, fmt, ap);
  if (n)
    buf[o.len < n ? o.len : n - 1] = '\0';
  return o.len;
}

int vsprintf(char *buf, const char *fmt, va_list ap) {
  return vsnprintf(buf, ULONG_MAX, fmt, ap);
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vfprintf(stdout, fmt, ap);
  va_end(ap);
  return n;
}

int fprintf(FILE *fp, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vfprintf(fp, fmt, ap);
  va_end(ap);
  return n;
}

int sprintf(char *buf, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsprintf(buf, fmt, ap);
  va_end(ap);
  return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return n;
}

void perror(const char *s) {
  if (s && *s)
    fprintf(stderr, "%s: ", s);
  fprintf(stderr, "%s\n", strerror(errno));
}

//
// Program termination
//

static void (*atexit_fns[32])(void);
static int atexit_count;

int atexit(void (*fn)(void)) {
  if (atexit_count == 32)
    return -1;
  atexit_fns[atexit_count++] = fn;
  return 0;
}

// Halt the CPU; the exit status becomes the stop code.
_Noreturn void _Exit(int status) {
  asm("            ld          ac, 3(ap)\n"
      "            hlt\n"
      "            jmp         .-1");
  for (;;);
}

_Noreturn void exit(int status) {
  while (atexit_count > 0)
    atexit_fns[--atexit_count]();
  flush_printer();
  _Exit(status);
}

_Noreturn void abort(void) {
  fputs("abort\n", stderr);
  flush_printer();
  _Exit(134);
}

_Noreturn void __assert_fail(const char *expr, const char *file, int line) {
  fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, expr);
  abort();
}

// Called by crt0 when the CPU takes an exception.
_Noreturn void __acr_trap(int code, int pc) {
  static char *names[16] = {
    "unimplemented instruction", "illegal instruction", "no such memory",
    "no such device", "protection fault (read)", "protection fault (write)",
    "protection fault (system)", "timer", "divide by zero", "no FPU",
    "exception 10", "exception 11", "exception 12", "exception 13",
    "machine check", "power failure",
  };
  // Avoid printf here so that every program doesn't need it.
  char digits[12];
  int n = 0;
  do {
    digits[n++] = '0' + (pc & 7);
    pc = (unsigned)pc >> 3;
  } while (pc);

  flush_printer();
  fputs("*** ACR-7000 exception: ", stderr);
  fputs(names[code & 15], stderr);
  fputs(" at PC 0", stderr);
  while (n)
    putchar(digits[--n]);
  putchar('\n');
  _Exit(128 + code);
}

char *getenv(const char *name) {
  return NULL;
}

//
// setjmp/longjmp
//

// jmp_buf: return address, caller's AP, and the caller's SP as it is
// when setjmp returns.
int setjmp(jmp_buf env) {
  asm("            ld          x1, 3(ap)\n"
      "            ld          x2, 0(ap)\n"
      "            st          x2, 0(x1)\n"
      "            ld          x2, 2(ap)\n"
      "            st          x2, 1(x1)\n"
      "            ldea        x2, 3(ap)\n"
      "            st          x2, 2(x1)");
  return 0;
}

_Noreturn void longjmp(jmp_buf env, int val) {
  asm("            ld          x1, 3(ap)\n"
      "            ld          ac, 4(ap)\n"
      "            movrn.rz    ac, ac\n"
      "            ldea        ac, 1\n"
      "            ld          x2, 0(x1)\n"
      "            ld          sp, 2(x1)\n"
      "            ld          ap, 1(x1)\n"
      "            jmp         0(x2)");
  for (;;);
}

//
// Memory allocation: a first-fit free list carved from the space between
// the end of the program and the stack.
//

extern char __heap_start[];

typedef struct Header Header;
struct Header {
  size_t size;  // in units of Header, including this one
  Header *next;
};

static Header base;
static Header *freep;
static char *brk;
static char *heap_start = __heap_start; // may be beyond 18-bit addresses

static char *stack_pointer(void) {
  asm("            movr        sp, ac");
}

static Header *more_core(size_t units) {
  if (units < 256)
    units = 256;
  if (!brk)
    brk = heap_start;

  // Leave room for the stack to keep growing.
  size_t words = units * sizeof(Header);
  if (stack_pointer() - brk < (long)words + 4096)
    return NULL;

  Header *h = (Header *)brk;
  brk += words;
  h->size = units;
  free(h + 1);
  return freep;
}

void *malloc(size_t n) {
  if (n == 0)
    n = 1;
  size_t units = (n + sizeof(Header) - 1) / sizeof(Header) + 1;

  Header *prev = freep;
  if (!prev) {
    base.next = freep = prev = &base;
    base.size = 0;
  }

  for (Header *p = prev->next;; prev = p, p = p->next) {
    if (p->size >= units) {
      if (p->size == units) {
        prev->next = p->next;
      } else {
        p->size -= units;
        p += p->size;
        p->size = units;
      }
      freep = prev;
      return p + 1;
    }
    if (p == freep && !(p = more_core(units))) {
      errno = ENOMEM;
      return NULL;
    }
  }
}

void free(void *ptr) {
  if (!ptr)
    return;

  Header *b = (Header *)ptr - 1;
  Header *p = freep;
  for (; !(b > p && b < p->next); p = p->next)
    if (p >= p->next && (b > p || b < p->next))
      break;

  if (b + b->size == p->next) {
    b->size += p->next->size;
    b->next = p->next->next;
  } else {
    b->next = p->next;
  }

  if (p + p->size == b) {
    p->size += b->size;
    p->next = b->next;
  } else {
    p->next = b;
  }
  freep = p;
}

void *calloc(size_t n, size_t size) {
  void *p = malloc(n * size);
  if (p)
    memset(p, 0, n * size);
  return p;
}

void *realloc(void *ptr, size_t n) {
  if (!ptr)
    return malloc(n);
  Header *h = (Header *)ptr - 1;
  size_t old = (h->size - 1) * sizeof(Header);
  if (n <= old)
    return ptr;
  void *p = malloc(n);
  if (p) {
    memcpy(p, ptr, old);
    free(ptr);
  }
  return p;
}

//
// Strings and memory
//

void *memcpy(void *dst, const void *src, size_t n) {
  char *d = dst;
  const char *s = src;
  while (n--)
    *d++ = *s++;
  return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
  char *d = dst;
  const char *s = src;
  if (d < s) {
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }
  return dst;
}

void *memset(void *p, int c, size_t n) {
  char *d = p;
  while (n--)
    *d++ = c;
  return p;
}

int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *p = a, *q = b;
  for (; n; n--, p++, q++)
    if (*p != *q)
      return *p < *q ? -1 : 1;
  return 0;
}

void *memchr(const void *p, int c, size_t n) {
  const unsigned char *s = p;
  for (; n; n--, s++)
    if (*s == (unsigned char)c)
      return (void *)s;
  return NULL;
}

size_t strlen(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

size_t strnlen(const char *s, size_t n) {
  size_t i = 0;
  while (i < n && s[i])
    i++;
  return i;
}

char *strcpy(char *dst, const char *src) {
  char *d = dst;
  while ((*d++ = *src++))
    ;
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t i = 0;
  for (; i < n && src[i]; i++)
    dst[i] = src[i];
  for (; i < n; i++)
    dst[i] = '\0';
  return dst;
}

char *strcat(char *dst, const char *src) {
  strcpy(dst + strlen(dst), src);
  return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
  char *d = dst + strlen(dst);
  while (n-- && *src)
    *d++ = *src++;
  *d = '\0';
  return dst;
}

int strcmp(const char *a, const char *b) {
  const unsigned char *p = (const unsigned char *)a;
  const unsigned char *q = (const unsigned char *)b;
  while (*p && *p == *q)
    p++, q++;
  return (*p > *q) - (*p < *q);
}

int strncmp(const char *a, const char *b, size_t n) {
  const unsigned char *p = (const unsigned char *)a;
  const unsigned char *q = (const unsigned char *)b;
  for (; n; n--, p++, q++) {
    if (*p != *q)
      return *p < *q ? -1 : 1;
    if (!*p)
      return 0;
  }
  return 0;
}

char *strchr(const char *s, int c) {
  for (;; s++) {
    if (*s == (char)c)
      return (char *)s;
    if (!*s)
      return NULL;
  }
}

char *strrchr(const char *s, int c) {
  const char *r = NULL;
  for (;; s++) {
    if (*s == (char)c)
      r = s;
    if (!*s)
      return (char *)r;
  }
}

char *strstr(const char *s, const char *t) {
  size_t n = strlen(t);
  for (; *s; s++)
    if (!strncmp(s, t, n))
      return (char *)s;
  return n ? NULL : (char *)s;
}

size_t strspn(const char *s, const char *accept) {
  size_t i = 0;
  while (s[i] && strchr(accept, s[i]))
    i++;
  return i;
}

size_t strcspn(const char *s, const char *reject) {
  size_t i = 0;
  while (s[i] && !strchr(reject, s[i]))
    i++;
  return i;
}

char *strpbrk(const char *s, const char *accept) {
  s += strcspn(s, accept);
  return *s ? (char *)s : NULL;
}

char *strtok(char *s, const char *delim) {
  static char *next;
  if (!s)
    s = next;
  if (!s)
    return NULL;
  s += strspn(s, delim);
  if (!*s) {
    next = NULL;
    return NULL;
  }
  char *end = s + strcspn(s, delim);
  if (*end)
    *end++ = '\0';
  else
    end = NULL;
  next = end;
  return s;
}

char *strdup(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (p)
    memcpy(p, s, n);
  return p;
}

char *strndup(const char *s, size_t n) {
  n = strnlen(s, n);
  char *p = malloc(n + 1);
  if (p) {
    memcpy(p, s, n);
    p[n] = '\0';
  }
  return p;
}

char *strerror(int err) {
  switch (err) {
  case 0: return "no error";
  case EDOM: return "domain error";
  case ERANGE: return "result out of range";
  case EINVAL: return "invalid argument";
  case ENOMEM: return "out of memory";
  }
  return "unknown error";
}

//
// Character classes (ASCII)
//

int isdigit(int c) { return '0' <= c && c <= '9'; }
int islower(int c) { return 'a' <= c && c <= 'z'; }
int isupper(int c) { return 'A' <= c && c <= 'Z'; }
int isalpha(int c) { return islower(c) || isupper(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int isspace(int c) { return c == ' ' || ('\t' <= c && c <= '\r'); }
int iscntrl(int c) { return (0 <= c && c < 32) || c == 127; }
int isprint(int c) { return 32 <= c && c < 127; }
int isgraph(int c) { return 32 < c && c < 127; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int tolower(int c) { return isupper(c) ? c + 32 : c; }
int toupper(int c) { return islower(c) ? c - 32 : c; }

//
// Numbers
//

int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }
long long llabs(long long n) { return n < 0 ? -n : n; }

div_t div(int a, int b) { return (div_t){a / b, a % b}; }
ldiv_t ldiv(long a, long b) { return (ldiv_t){a / b, a % b}; }
lldiv_t lldiv(long long a, long long b) { return (lldiv_t){a / b, a % b}; }

static unsigned long parse_unsigned(const char *s, char **end, int base,
                                    int *neg, int *overflow) {
  const char *p = s;
  while (isspace(*p))
    p++;

  *neg = 0;
  if (*p == '-' || *p == '+')
    *neg = (*p++ == '-');

  if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
      isxdigit(p[2])) {
    p += 2;
    base = 16;
  } else if (base == 0) {
    base = (*p == '0') ? 8 : 10;
  }

  unsigned long v = 0;
  const char *start = p;
  *overflow = 0;
  for (;; p++) {
    int d;
    if (isdigit(*p))
      d = *p - '0';
    else if (isalpha(*p))
      d = tolower(*p) - 'a' + 10;
    else
      break;
    if (d >= base)
      break;
    if (v > (ULONG_MAX - d) / base)
      *overflow = 1;
    v = v * base + d;
  }

  if (end)
    *end = (char *)(p == start ? s : p);
  return v;
}

unsigned long strtoul(const char *s, char **end, int base) {
  int neg, overflow;
  unsigned long v = parse_unsigned(s, end, base, &neg, &overflow);
  if (overflow) {
    errno = ERANGE;
    return ULONG_MAX;
  }
  return neg ? -v : v;
}

long strtol(const char *s, char **end, int base) {
  int neg, overflow;
  unsigned long v = parse_unsigned(s, end, base, &neg, &overflow);
  if (overflow || v > (unsigned long)LONG_MAX + neg) {
    errno = ERANGE;
    return neg ? LONG_MIN : LONG_MAX;
  }
  return neg ? -(long)v : (long)v;
}

long long strtoll(const char *s, char **end, int base) {
  return strtol(s, end, base);
}

unsigned long long strtoull(const char *s, char **end, int base) {
  return strtoul(s, end, base);
}

int atoi(const char *s) { return strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }
long long atoll(const char *s) { return strtol(s, NULL, 10); }

static unsigned long rand_state = 1;

int rand(void) {
  rand_state = rand_state * 1103515245 + 12345;
  return (rand_state >> 1) & RAND_MAX;
}

void srand(unsigned seed) {
  rand_state = seed;
}

//
// Sorting and searching
//

static void swap_words(char *a, char *b, size_t n) {
  while (n--) {
    char t = *a;
    *a++ = *b;
    *b++ = t;
  }
}

void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *)) {
  // Shell sort: small, and no recursion to deepen the stack.
  char *a = base;
  for (size_t gap = n / 2; gap > 0; gap /= 2)
    for (size_t i = gap; i < n; i++)
      for (size_t j = i; j >= gap && cmp(a + (j - gap) * size, a + j * size) > 0; j -= gap)
        swap_words(a + (j - gap) * size, a + j * size, size);
}

void *bsearch(const void *key, const void *base, size_t n, size_t size,
              int (*cmp)(const void *, const void *)) {
  const char *a = base;
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    int c = cmp(key, a + mid * size);
    if (c == 0)
      return (void *)(a + mid * size);
    if (c < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  return NULL;
}
