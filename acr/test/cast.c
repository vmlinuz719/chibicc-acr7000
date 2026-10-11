#include "test.h"  // ACR-7000 adaptation of test/cast.c (see acr/test/adapt.py)

int main() {
  ASSERT(8590066177, (int)8590066177);
  ASSERT(-130559, (short)8590066177);
  ASSERT(1, (char)8590066177);
  ASSERT(1, (long)1);
  ASSERT(0, (long)&*(int *)0);
  ASSERT(134218240, ({ int x=512; *(char *)&x=1; x; }));
  ASSERT(5, ({ int x=5; long y=(long)&x; *(int*)y; }));

  (void)1;

  ASSERT(255, (char)255);
  ASSERT(255, (signed char)255);
  ASSERT(255, (unsigned char)255);
  ASSERT(65535, (short)65535);
  ASSERT(65535, (unsigned short)65535);
  ASSERT(4294967295, (int)0xffffffff);
  ASSERT(0xffffffff, (unsigned)0xffffffff);

  ASSERT(1, -1<1);
  ASSERT(0, -1<(unsigned)1);
  ASSERT(254, (char)127+(char)127);
  ASSERT(65534, (short)32767+(short)32767);
  ASSERT(-1, -1>>1);
  ASSERT(-1, (unsigned long)-1);
  ASSERT(34359738367, ((unsigned)-1)>>1);
  ASSERT(-50, (-100)/2);
  ASSERT(34359738318, ((unsigned)-100)/2);
  ASSERT(34359738318, ((unsigned long)-100)/2);
  ASSERT(687194767, ((long)-1)/(unsigned)100);
  ASSERT(-2, (-100)%7);
  ASSERT(6, ((unsigned)-100)%7);
  ASSERT(0, ((unsigned long)-100)%9);

  ASSERT(65535, (int)(unsigned short)65535);
  ASSERT(65535, ({ unsigned short x = 65535; x; }));
  ASSERT(65535, ({ unsigned short x = 65535; (int)x; }));

  ASSERT(65535, ({ typedef short T; T x = 65535; (int)x; }));
  ASSERT(65535, ({ typedef unsigned short T; T x = 65535; (int)x; }));


  printf("OK\n");
  return 0;
}
