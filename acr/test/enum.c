#include "test.h"  // ACR-7000 adaptation of test/enum.c (see acr/test/adapt.py)

int main() {
  ASSERT(0, ({ enum { zero, one, two }; zero; }));
  ASSERT(1, ({ enum { zero, one, two }; one; }));
  ASSERT(2, ({ enum { zero, one, two }; two; }));
  ASSERT(5, ({ enum { five=5, six, seven }; five; }));
  ASSERT(6, ({ enum { five=5, six, seven }; six; }));
  ASSERT(0, ({ enum { zero, five=5, three=3, four }; zero; }));
  ASSERT(5, ({ enum { zero, five=5, three=3, four }; five; }));
  ASSERT(3, ({ enum { zero, five=5, three=3, four }; three; }));
  ASSERT(4, ({ enum { zero, five=5, three=3, four }; four; }));
  ASSERT(1, ({ enum { zero, one, two } x; sizeof(x); }));
  ASSERT(1, ({ enum t { zero, one, two }; enum t y; sizeof(y); }));

  printf("OK\n");
  return 0;
}
