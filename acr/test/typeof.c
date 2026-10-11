#include "test.h"  // ACR-7000 adaptation of test/typeof.c (see acr/test/adapt.py)

int main() {
  ASSERT(3, ({ typeof(int) x=3; x; }));
  ASSERT(3, ({ typeof(1) x=3; x; }));
  ASSERT(4, ({ int x; typeof(x) y; sizeof(y); }));
  ASSERT(4, ({ int x; typeof(&x) y; sizeof(y); }));
  ASSERT(4, ({ typeof("foo") x; sizeof(x); }));
  ASSERT(12, sizeof(typeof(struct { int a,b,c; })));

  printf("OK\n");
  return 0;
}
