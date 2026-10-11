// Tests specific to the ACR-7000 port: 36-bit arithmetic, the code
// generator's special cases, the linker's fixups and zero-fill, and the
// C library.
#include "test.h"
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int sprintf(char *buf, char *fmt, ...);
int snprintf(char *buf, long n, char *fmt, ...);
long strtol(char *s, char **end, int base);
unsigned long strtoul(char *s, char **end, int base);

// Constants that need ldea, ldeas(+addea) and an inline literal.
long k_small(void) { return -131072; }
long k_ldeas(void) { return 0x123456789; }
long k_ldeas_neg(void) { return -0x123456789; }
long k_literal(void) { return 0x543210fed; }
long k_sign(void) { return 0400000000000; }

int shl(int x, int n) { return x << n; }
int sar(int x, int n) { return x >> n; }
unsigned shr(unsigned x, int n) { return x >> n; }

int lt(int a, int b) { return a < b; }
int ult(unsigned a, unsigned b) { return a < b; }
int le(int a, int b) { return a <= b; }
int ule(unsigned a, unsigned b) { return a <= b; }

int branch_lt(int a, int b) { if (a < b) return 1; return 0; }
int branch_ule(unsigned a, unsigned b) { if (a <= b) return 1; return 0; }
int branch_eq_imm(int a) { if (a == 4096) return 1; return 0; }
int branch_eq_imm2(int a) { if (a == -4095) return 1; return 0; }
int branch_eq_big(int a) { if (a == 4097) return 1; return 0; }
int branch_ne_big(long a) { if (a != 0x123456789) return 1; return 0; }

int neg(int x) { if (x < 0) return 1; return 0; }
int nonneg(int x) { if (x >= 0) return 1; return 0; }
int pos(int x) { if (x > 0) return 1; return 0; }
int count_neg(int *a, int n) { int c = 0; for (int i = 0; i < n; i++) if (a[i] < 0) c++; return c; }

int sw(int x) {
  switch (x) {
  case -5: return 1;
  case 0: return 2;
  case 4096: return 3;
  case 100000: return 4;
  case 0x123456789: return 5;
  case 10 ... 20: return 6;
  default: return 7;
  }
}

typedef struct { int a, b, c, d, e, f, g, h; } Big;
typedef struct { int x; } Small;

Big make_big(int n) {
  Big b = {n, n + 1, n + 2, n + 3, n + 4, n + 5, n + 6, n + 7};
  return b;
}

Small make_small(int n) { return (Small){n * 2}; }

int sum_big(Big b) { return b.a + b.b + b.c + b.d + b.e + b.f + b.g + b.h; }

int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int sum_va(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int s = 0;
  while (n--)
    s += va_arg(ap, int);
  va_end(ap);
  return s;
}

// Global initializers with address arithmetic (crt0 fixups).
int garr[10] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
int *gp3 = &garr[3];
int *gp_end = garr + 10;
char *strs[] = {"zero", "one", "two" + 1};
struct { int pad; int *p; } gs = {1, &garr[7]};

// Zero-filled data: tentative, static and a long zero run.
int gzero[300];
static int szero[50];
int gsparse[100] = {1, [99] = 2};

// Large zero-filled objects are placed after the program and reached
// through address words.
static int bigarr[150000];
struct { int pad[2000]; int x; } bigs;
int bigarr2[2000] = {[1999] = 7};

struct bits { unsigned u : 5; int s : 7; unsigned w : 24; };

enum big { BIG = 0x123456789, BIGGER };

jmp_buf jb;

void do_longjmp(int n) {
  if (n == 0)
    longjmp(jb, 42);
  do_longjmp(n - 1);
}

int cmp_int(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}

int main() {
  // Words are 36 bits.
  ASSERT(36, CHAR_BIT);
  ASSERT(1, sizeof(long long));
  ASSERT(1, INT_MAX == 34359738367);
  ASSERT(1, INT_MIN < 0 && (unsigned)INT_MIN - 1 == INT_MAX);
  ASSERT(1, (unsigned)-1 == 68719476735);
  ASSERT(256, ({ char c = 255; c + 1; }));
  ASSERT(0, ({ unsigned u = UINT_MAX; u + 1; }));

  // Constant loading
  ASSERT(1, k_small() == -131072);
  ASSERT(1, k_ldeas() == 0x123456789);
  ASSERT(1, k_ldeas_neg() == -0x123456789);
  ASSERT(1, k_literal() == 0x543210fed);
  ASSERT(1, k_sign() == INT_MIN);

  // Variable and constant shifts
  ASSERT(48, shl(3, 4));
  ASSERT(1, shl(1, 35) == INT_MIN);
  ASSERT(-4, sar(-16, 2));
  ASSERT(-1, sar(-1, 35));
  ASSERT(4, sar(16, 2));
  ASSERT(15, shr(-1, 32));
  ASSERT(1, shr(-1, 35));
  ASSERT(7, shr(7, 0));
  ASSERT(-8, ({ int x = -64; x >> 3; }));
  ASSERT(8, ({ unsigned x = 64; x >> 3; }));
  ASSERT(1, ({ unsigned x = 0400000000000; x >> 35; }));

  // Comparisons, signed and unsigned, as values and as branches
  ASSERT(1, lt(-1, 1));
  ASSERT(0, ult(-1, 1));
  ASSERT(1, lt(INT_MIN, INT_MAX));
  ASSERT(0, lt(INT_MAX, INT_MIN));
  ASSERT(1, le(5, 5));
  ASSERT(0, le(6, 5));
  ASSERT(1, ule(5, -1));
  ASSERT(1, branch_lt(-1, 1));
  ASSERT(0, branch_lt(1, -1));
  ASSERT(0, branch_lt(3, 3));
  ASSERT(1, branch_ule(3, 3));
  ASSERT(0, branch_ule(-1, 3));
  ASSERT(1, branch_eq_imm(4096));
  ASSERT(0, branch_eq_imm(4095));
  ASSERT(1, branch_eq_imm2(-4095));
  ASSERT(1, branch_eq_big(4097));
  ASSERT(0, branch_eq_big(4096));
  ASSERT(0, branch_ne_big(0x123456789));
  ASSERT(1, branch_ne_big(0x123456788));
  ASSERT(1, ({ int a = 3, b = 4; a < b && b < 5 || a == 9; }));
  ASSERT(0, ({ int a = 3, b = 4; !(a < b) || b > 5; }));

  ASSERT(1, neg(-1) && neg(INT_MIN) && !neg(0) && !neg(1) && !neg(INT_MAX));
  ASSERT(1, !nonneg(-1) && !nonneg(INT_MIN) && nonneg(0) && nonneg(1) && nonneg(INT_MAX));
  ASSERT(1, !pos(-1) && !pos(INT_MIN) && !pos(0) && pos(1) && pos(INT_MAX));
  ASSERT(3, ({ int a[6] = {-1, 0, INT_MIN, 5, -7, INT_MAX}; count_neg(a, 6); }));
  ASSERT(1, ({ int x = -5; x < 0 ? 1 : 2; }));
  ASSERT(2, ({ int x = 0; x < 0 ? 1 : 2; }));
  ASSERT(1, ({ unsigned x = -5; x >= 0; }));

  // Multiplication, division and remainder
  ASSERT(-21, ({ int a = 7, b = -3; a * b; }));
  ASSERT(-2, ({ int a = 7, b = -3; a / b; }));
  ASSERT(1, ({ int a = 7, b = -3; a % b; }));
  ASSERT(-3, ({ int a = -7; a / 2; }));
  ASSERT(-1, ({ int a = -7; a % 2; }));
  ASSERT(3, ({ unsigned a = 7; a / 2; }));
  ASSERT(1, ({ unsigned a = 7; a % 2; }));
  ASSERT(34359738367, ({ unsigned a = -1; a / 2; }));
  ASSERT(15, ({ unsigned a = -1; a % 16; }));
  ASSERT(1, ({ unsigned a = -1; a % 8192; }) == 8191);
  ASSERT(1, ({ unsigned a = -1; a % 16384; }) == 16383);
  ASSERT(40, ({ int a = 5; a * 8; }));
  ASSERT(0, ({ int a = 5; a * 0; }));
  ASSERT(15, ({ int a = 5, b = 3; int *p = &b; a * *p; }));
  ASSERT(1, ({ long a = 0x100000; a * a; }) == 0);

  // switch with immediate, large and range cases
  ASSERT(1, sw(-5));
  ASSERT(2, sw(0));
  ASSERT(3, sw(4096));
  ASSERT(4, sw(100000));
  ASSERT(5, sw(0x123456789));
  ASSERT(6, sw(10));
  ASSERT(6, sw(20));
  ASSERT(7, sw(21));
  ASSERT(7, sw(9));

  ASSERT(1, BIG == 0x123456789 && BIGGER == 0x12345678a);

  // Structs: returned, passed and copied
  ASSERT(28, sum_big(make_big(0)));
  ASSERT(15, make_big(8).h);
  ASSERT(14, make_small(7).x);
  ASSERT(15, ({ Big a = make_big(1), b; b = a; b.a + b.h + sum_big(b) - 30; }));
  ASSERT(6765, fib(20));
  ASSERT(15, sum_va(5, 1, 2, 3, 4, 5));

  // Pointers through indirect addressing
  ASSERT(3, ({ int x[3] = {1, 2, 3}; int *p = x + 2; *p; }));
  ASSERT(9, ({ int x[3] = {1, 2, 3}; int *p = x; *p = 9; x[0]; }));
  ASSERT(5, ({ struct { int a, b; } s = {4, 5}, *p = &s; p->a + p->b - 4; }));
  ASSERT(8, ({ struct { int a, b; } s = {4, 5}, *p = &s; p->a += 4; s.a; }));
  ASSERT(10, ({ int x[2] = {5, 6}; int *p = x; *p++ += 5; x[0]; }));
  ASSERT(6, ({ int x[2] = {5, 6}; int *p = x; *++p; }));

  // Address constants patched by crt0
  ASSERT(13, *gp3);
  ASSERT(10, gp_end - garr);
  ASSERT(0, strcmp(strs[1], "one"));
  ASSERT(0, strcmp(strs[2], "wo"));
  ASSERT(17, *gs.p);

  // Zero-filled data
  ASSERT(0, gzero[0] + gzero[299] + szero[49]);
  ASSERT(1, gsparse[0]);
  ASSERT(0, gsparse[50]);
  ASSERT(2, gsparse[99]);

  ASSERT(5, ({ bigarr[149999] = 5; bigarr[0] = 1; bigarr[149999] + bigarr[1]; }));
  ASSERT(9, ({ int *p = bigarr + 140000; *p = 9; bigarr[140000]; }));
  ASSERT(3, ({ bigs.x = 3; bigs.pad[0] + bigs.x; }));
  ASSERT(7, bigarr2[1999]);

  // Bit-fields
  ASSERT(31, ({ struct bits b = {0}; b.u = 63; b.u; }));
  ASSERT(-64, ({ struct bits b = {0}; b.s = 64; b.s; }));
  ASSERT(0xffffff, ({ struct bits b = {0}; b.w = -1; b.w; }));
  ASSERT(3, ({ struct bits b = {1, 2, 3}; b.u += 2; b.u; }));
  ASSERT(5, ({ struct bits b = {1, 2, 3}; b.w += 2; b.w; }));
  ASSERT(2, ({ struct bits b = {1, 2, 3}; b.s; }));

  // Labels as values
  ASSERT(2, ({ void *l = &&two; int r = 1; goto *l; one: r = 1; goto out; two: r = 2; out:; r; }));

  // setjmp and longjmp
  ASSERT(42, ({ int r = setjmp(jb); if (!r) do_longjmp(5); r; }));

  // The C library
  ASSERT(0, ({ char buf[64]; sprintf(buf, "%d|%5s|%-4d|%x|%o|%c", -12, "ab", 7, 0xfff, 0777777777777, 'z'); strcmp(buf, "-12|   ab|7   |fff|777777777777|z"); }));
  ASSERT(0, ({ char buf[64]; sprintf(buf, "%u %d", -1, INT_MIN); strcmp(buf, "68719476735 -34359738368"); }));
  ASSERT(5, ({ char buf[4]; snprintf(buf, 4, "%s", "hello"); }));
  ASSERT(0, ({ char buf[4]; snprintf(buf, 4, "%s", "hello"); strcmp(buf, "hel"); }));
  ASSERT(-255, strtol("-0xff", NULL, 0));
  ASSERT(511, strtol("0777", NULL, 0));
  ASSERT(1, strtoul("68719476735", NULL, 10) == UINT_MAX);
  ASSERT(1, ({ int *p = malloc(1000 * sizeof(int)); p[999] = 3; int *q = malloc(10); int ok = p && q && q != p; free(p); free(q); ok; }));
  ASSERT(1, ({ char *p = malloc(5); strcpy(p, "abcd"); p = realloc(p, 5000); !strcmp(p, "abcd"); }));
  ASSERT(0, ({ int *p = calloc(100, sizeof(int)); p[0] + p[99]; }));
  ASSERT(1, ({ int a[6] = {5, 3, 9, -1, 0, 3}; qsort(a, 6, sizeof(int), cmp_int); a[0] == -1 && a[1] == 0 && a[3] == 3 && a[5] == 9; }));
  ASSERT(0, memcmp("abc", "abc", 3));
  ASSERT(6, ({ char *s = "hello world"; strstr(s, "wor") - s; }));

  printf("OK\n");
  return 0;
}
