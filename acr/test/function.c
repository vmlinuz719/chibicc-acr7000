#include "test.h"  // ACR-7000 adaptation of test/function.c (see acr/test/adapt.py)

int ret3(void) {
  return 3;
  return 5;
}

int add2(int x, int y) {
  return x + y;
}

int sub2(int x, int y) {
  return x - y;
}

int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

int addx(int *x, int y) {
  return *x + y;
}

int sub_char(char a, char b, char c) {
  return a - b - c;
}

int fib(int x) {
  if (x<=1)
    return 1;
  return fib(x-1) + fib(x-2);
}

int sub_long(long a, long b, long c) {
  return a - b - c;
}

int sub_short(short a, short b, short c) {
  return a - b - c;
}

int g1;

int *g1_ptr(void) { return &g1; }
char int_to_char(int x) { return x; }

int div_long(long a, long b) {
  return a / b;
}

_Bool bool_fn_add(_Bool x) { return x + 1; }
_Bool bool_fn_sub(_Bool x) { return x - 1; }

static int static_fn(void) { return 3; }

int param_decay(int x[]) { return x[0]; }

int counter() {
  static int i;
  static int j = 1+1;
  return i++ + j++;
}

void ret_none() {
  return;
}

_Bool true_fn();
_Bool false_fn();
char char_fn();
short short_fn();

unsigned char uchar_fn();
unsigned short ushort_fn();

char schar_fn();
short sshort_fn();

int add_all(int n, ...);

typedef struct {
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} __va_elem;

typedef char *va_list;

int add_all(int n, ...);
int sprintf(char *buf, char *fmt, ...);
int vsprintf(char *buf, char *fmt, va_list ap);

char *fmt(char *buf, char *fmt, ...) {
  va_list ap;
  ap = (char *)(&fmt + 1);
  vsprintf(buf, fmt, ap);
}



int (*fnptr(int (*fn)(int n, ...)))(int, ...) {
  return fn;
}

int param_decay2(int x()) { return x(); }

char *func_fn(void) {
  return __func__;
}

char *function_fn(void) {
  return __FUNCTION__;
}

int add10_int(int x1, int x2, int x3, int x4, int x5, int x6, int x7, int x8, int x9, int x10);

int many_args1(int a, int b, int c, int d, int e, int f, int g, int h) {
  return g / h;
}


typedef struct { int a,b; short c; char d; } Ty4;
typedef struct { int a; int b; int c; } Ty5;
typedef struct { unsigned char a[3]; } Ty6;
typedef struct { long a, b, c; } Ty7;

int struct_test4(Ty4 x, int n);
int struct_test6(Ty6 x, int n);
int struct_test7(Ty7 x, int n);

int struct_test14(Ty4 x, int n) {
  switch (n) {
  case 0: return x.a;
  case 1: return x.b;
  case 2: return x.c;
  default: return x.d;
  }
}

int struct_test15(Ty5 x, int n) {
  switch (n) {
  case 0: return x.a;
  case 1: return x.b;
  default: return x.c;
  }
}

typedef struct { unsigned char a[10]; } Ty20;
typedef struct { unsigned char a[20]; } Ty21;

Ty4 struct_test24(void);
Ty6 struct_test26(void);
Ty20 struct_test27(void);
Ty21 struct_test28(void);

Ty4 struct_test34(void) {
  return (Ty4){10, 20, 30, 40};
}

Ty5 struct_test35(void) {
  return (Ty5){10, 20, 30};
}

Ty6 struct_test36(void) {
  return (Ty6){10, 20, 30};
}

Ty20 struct_test37(void) {
  return (Ty20){10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
}

Ty21 struct_test38(void) {
  return (Ty21){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
}

inline int inline_fn(void) {
  return 3;
}


int main() {
  ASSERT(3, ret3());
  ASSERT(8, add2(3, 5));
  ASSERT(2, sub2(5, 3));
  ASSERT(21, add6(1,2,3,4,5,6));
  ASSERT(66, add6(1,2,add6(3,4,5,6,7,8),9,10,11));
  ASSERT(136, add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16));

  ASSERT(7, add2(3,4));
  ASSERT(1, sub2(4,3));
  ASSERT(55, fib(9));

  ASSERT(1, ({ sub_char(7, 3, 3); }));

  ASSERT(1, sub_long(7, 3, 3));
  ASSERT(1, sub_short(7, 3, 3));

  g1 = 3;

  ASSERT(3, *g1_ptr());
  ASSERT(261, int_to_char(261));
  ASSERT(261, int_to_char(261));
  ASSERT(-5, div_long(-10, 2));

  ASSERT(1, bool_fn_add(3));
  ASSERT(0, bool_fn_sub(3));
  ASSERT(1, bool_fn_add(-3));
  ASSERT(0, bool_fn_sub(-3));
  ASSERT(1, bool_fn_add(0));
  ASSERT(1, bool_fn_sub(0));

  ASSERT(3, static_fn());

  ASSERT(3, ({ int x[2]; x[0]=3; param_decay(x); }));

  ASSERT(2, counter());
  ASSERT(4, counter());
  ASSERT(6, counter());

  ret_none();

  ASSERT(513, true_fn());
  ASSERT(512, false_fn());
  ASSERT(515, char_fn());
  ASSERT(131077, short_fn());

  ASSERT(6, add_all(3,1,2,3));
  ASSERT(5, add_all(4,1,2,3,-1));

  { char buf[100]; fmt(buf, "%d %d %s", 1, 2, "foo"); printf("%s\n", buf); }

  ASSERT(0, ({ char buf[100]; sprintf(buf, "%d %d %s", 1, 2, "foo"); strcmp("1 2 foo", buf); }));

  ASSERT(0, ({ char buf[100]; fmt(buf, "%d %d %s", 1, 2, "foo"); strcmp("1 2 foo", buf); }));

  ASSERT(2043, uchar_fn());
  ASSERT(2097144, ushort_fn());
  ASSERT(2043, schar_fn());
  ASSERT(2097144, sshort_fn());





  ASSERT(5, (add2)(2,3));
  ASSERT(5, (&add2)(2,3));
  ASSERT(7, ({ int (*fn)(int,int) = add2; fn(2,5); }));
  ASSERT(6, fnptr(add_all)(3, 1, 2, 3));

  ASSERT(3, param_decay2(ret3));

  ASSERT(5, sizeof(__func__));
  ASSERT(0, strcmp("main", __func__));
  ASSERT(0, strcmp("func_fn", func_fn()));
  ASSERT(0, strcmp("main", __FUNCTION__));
  ASSERT(0, strcmp("function_fn", function_fn()));


  ASSERT(55, add10_int(1,2,3,4,5,6,7,8,9,10));


  ASSERT(4, many_args1(1,2,3,4,5,6,40,10));

  ASSERT(10, ({ Ty4 x={10,20,30,40}; struct_test4(x, 0); }));
  ASSERT(20, ({ Ty4 x={10,20,30,40}; struct_test4(x, 1); }));
  ASSERT(30, ({ Ty4 x={10,20,30,40}; struct_test4(x, 2); }));
  ASSERT(40, ({ Ty4 x={10,20,30,40}; struct_test4(x, 3); }));


  ASSERT(10, ({ Ty6 x={10,20,30}; struct_test6(x, 0); }));
  ASSERT(20, ({ Ty6 x={10,20,30}; struct_test6(x, 1); }));
  ASSERT(30, ({ Ty6 x={10,20,30}; struct_test6(x, 2); }));

  ASSERT(10, ({ Ty7 x={10,20,30}; struct_test7(x, 0); }));
  ASSERT(20, ({ Ty7 x={10,20,30}; struct_test7(x, 1); }));
  ASSERT(30, ({ Ty7 x={10,20,30}; struct_test7(x, 2); }));

  ASSERT(10, ({ Ty4 x={10,20,30,40}; struct_test14(x, 0); }));
  ASSERT(20, ({ Ty4 x={10,20,30,40}; struct_test14(x, 1); }));
  ASSERT(30, ({ Ty4 x={10,20,30,40}; struct_test14(x, 2); }));
  ASSERT(40, ({ Ty4 x={10,20,30,40}; struct_test14(x, 3); }));

  ASSERT(10, ({ Ty5 x={10,20,30}; struct_test15(x, 0); }));
  ASSERT(20, ({ Ty5 x={10,20,30}; struct_test15(x, 1); }));
  ASSERT(30, ({ Ty5 x={10,20,30}; struct_test15(x, 2); }));

  ASSERT(10, struct_test24().a);
  ASSERT(20, struct_test24().b);
  ASSERT(30, struct_test24().c);
  ASSERT(40, struct_test24().d);


  ASSERT(10, struct_test26().a[0]);
  ASSERT(20, struct_test26().a[1]);
  ASSERT(30, struct_test26().a[2]);

  ASSERT(10, struct_test27().a[0]);
  ASSERT(60, struct_test27().a[5]);
  ASSERT(100, struct_test27().a[9]);

  ASSERT(1, struct_test28().a[0]);
  ASSERT(5, struct_test28().a[4]);
  ASSERT(10, struct_test28().a[9]);
  ASSERT(15, struct_test28().a[14]);
  ASSERT(20, struct_test28().a[19]);

  ASSERT(10, struct_test34().a);
  ASSERT(20, struct_test34().b);
  ASSERT(30, struct_test34().c);
  ASSERT(40, struct_test34().d);

  ASSERT(10, struct_test35().a);
  ASSERT(20, struct_test35().b);
  ASSERT(30, struct_test35().c);

  ASSERT(10, struct_test36().a[0]);
  ASSERT(20, struct_test36().a[1]);
  ASSERT(30, struct_test36().a[2]);

  ASSERT(10, struct_test37().a[0]);
  ASSERT(60, struct_test37().a[5]);
  ASSERT(100, struct_test37().a[9]);

  ASSERT(1, struct_test38().a[0]);
  ASSERT(5, struct_test38().a[4]);
  ASSERT(10, struct_test38().a[9]);
  ASSERT(15, struct_test38().a[14]);
  ASSERT(20, struct_test38().a[19]);

  ASSERT(5, (***add2)(2,3));

  ASSERT(3, inline_fn());





  printf("OK\n");
}
