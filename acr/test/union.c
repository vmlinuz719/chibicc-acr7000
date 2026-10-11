#include "test.h"  // ACR-7000 adaptation of test/union.c (see acr/test/adapt.py)

int main() {
  ASSERT(6, ({ union { int a; char b[6]; } x; sizeof(x); }));
  ASSERT(515, ({ union { int a; char b[4]; } x; x.a = 515; x.b[0]; }));
  ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[1]; }));
  ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[2]; }));
  ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[3]; }));

  ASSERT(3, ({ union {int a,b;} x,y; x.a=3; y.a=5; y=x; y.a; }));
  ASSERT(3, ({ union {struct {int a,b;} c;} x,y; x.c.b=3; y.c.b=5; y=x; y.c.b; }));

  ASSERT(0xdeadbeef, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.a; }));
  ASSERT(0, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.b; }));
  ASSERT(0, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.c; }));
  ASSERT(0, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.d; }));

  ASSERT(3, ({struct { union { int a,b; }; union { int c,d; }; } x; x.a=3; x.b; }));
  ASSERT(5, ({struct { union { int a,b; }; union { int c,d; }; } x; x.d=5; x.c; }));

  printf("OK\n");
  return 0;
}
