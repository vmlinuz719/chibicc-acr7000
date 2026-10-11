#include "test.h"  // ACR-7000 adaptation of test/decl.c (see acr/test/adapt.py)

int main() {
  ASSERT(1, ({ char x; sizeof(x); }));
  ASSERT(1, ({ short int x; sizeof(x); }));
  ASSERT(1, ({ int short x; sizeof(x); }));
  ASSERT(1, ({ int x; sizeof(x); }));
  ASSERT(1, ({ long int x; sizeof(x); }));
  ASSERT(1, ({ int long x; sizeof(x); }));

  ASSERT(1, ({ long long x; sizeof(x); }));

  ASSERT(0, ({ _Bool x=0; x; }));
  ASSERT(1, ({ _Bool x=1; x; }));
  ASSERT(1, ({ _Bool x=2; x; }));
  ASSERT(1, (_Bool)1);
  ASSERT(1, (_Bool)2);
  ASSERT(1, (_Bool)(char)256);

  printf("OK\n");
  return 0;
}
