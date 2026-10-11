#include "test.h"  // ACR-7000 adaptation of test/alignof.c (see acr/test/adapt.py)

char g3;
int g4;
long g5;
char g6;

int main() {
  ASSERT(1, _Alignof(char));
  ASSERT(1, _Alignof(short));
  ASSERT(1, _Alignof(int));
  ASSERT(1, _Alignof(long));
  ASSERT(1, _Alignof(long long));
  ASSERT(1, _Alignof(char[3]));
  ASSERT(1, _Alignof(int[3]));
  ASSERT(1, _Alignof(struct {char a; char b;}[2]));
  ASSERT(1, _Alignof(struct {char a; long b;}[2]));

  ASSERT(1, ({ _Alignas(char) char x, y; &y-&x; }));
  ASSERT(1, ({ _Alignas(long) char x, y; &y-&x; }));
  ASSERT(16, ({ struct { _Alignas(16) char x, y; } a; &a.y-&a.x; }));
  ASSERT(8, ({ struct T { _Alignas(8) char a; }; _Alignof(struct T); }));

  ASSERT(0, (long)(char *)&g4 % 1);
  ASSERT(0, (long)(char *)&g5 % 1);

  ASSERT(1, ({ char x; _Alignof(x); }));
  ASSERT(1, ({ int x; _Alignof(x); }));
  ASSERT(1, ({ char x; _Alignof x; }));
  ASSERT(1, ({ int x; _Alignof x; }));

  ASSERT(1, _Alignof(char) << 31 >> 31);
  ASSERT(1, _Alignof(char) << 35 >> 35);
  ASSERT(1, ({ char x; _Alignof(x) << 35 >> 35; }));


  printf("OK\n");
  return 0;
}
