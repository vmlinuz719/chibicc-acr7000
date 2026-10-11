#include "test.h"  // ACR-7000 adaptation of test/vla.c (see acr/test/adapt.py)

int main() {
  ASSERT(5, ({ int n=5; int x[n]; sizeof(x); }));
  ASSERT(96, ({ int m=5, n=8; int x[m+1][n*2]; sizeof(x); }));

  ASSERT(1, ({ char n=10; int (*x)[n][n+2]; sizeof(x); }));
  ASSERT(120, ({ char n=10; int (*x)[n][n+2]; sizeof(*x); }));
  ASSERT(12, ({ char n=10; int (*x)[n][n+2]; sizeof(**x); }));
  ASSERT(1, ({ char n=10; int (*x)[n][n+2]; sizeof(***x); }));

  ASSERT(15, ({ char n=3; int x[5][n]; sizeof(x); }));
  ASSERT(3, ({ char n=3; int x[5][n]; sizeof(*x); }));

  ASSERT(15, ({ char n=3; int x[n][5]; sizeof(x); }));
  ASSERT(5, ({ char n=3; int x[n][5]; sizeof(*x); }));

  ASSERT(82, ({ int n=10; int x[n+1][n+6]; int *p=x; for (int i = 0; i<sizeof(x)/sizeof(int); i++) p[i]=i; x[5][2]; }));
  ASSERT(82, ({ int n=10; int x[n+1][n+6]; int *p=x; for (int i = 0; i<sizeof(x)/sizeof(int); i++) p[i]=i; x[5][2]; }));
  ASSERT(82, ({ int n=10; int x[n+1][n+6]; int *p=x; for (int i = 0; i<sizeof(x)/sizeof(int); i++) p[i]=i; x[5][2]; }));

  ASSERT(10, ({ int n=5; sizeof(char[2][n]); }));

  printf("OK\n");
  return 0;
}
