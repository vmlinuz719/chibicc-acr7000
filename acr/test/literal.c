#include "test.h"  // ACR-7000 adaptation of test/literal.c (see acr/test/adapt.py)

int main() {
  ASSERT(97, 'a');
  ASSERT(10, '\n');
  ASSERT(128, '\x80');

  ASSERT(511, 0777);
  ASSERT(0, 0x0);
  ASSERT(10, 0xa);
  ASSERT(10, 0XA);
  ASSERT(48879, 0xbeef);
  ASSERT(48879, 0xBEEF);
  ASSERT(48879, 0XBEEF);
  ASSERT(0, 0b0);
  ASSERT(1, 0b1);
  ASSERT(47, 0b101111);
  ASSERT(47, 0B101111);

  ASSERT(4, sizeof(0));
  ASSERT(4, sizeof(0L));
  ASSERT(4, sizeof(0LU));
  ASSERT(4, sizeof(0UL));
  ASSERT(4, sizeof(0LL));
  ASSERT(4, sizeof(0LLU));
  ASSERT(4, sizeof(0Ull));
  ASSERT(4, sizeof(0l));
  ASSERT(4, sizeof(0ll));
  ASSERT(4, sizeof(0x0L));
  ASSERT(4, sizeof(0b0L));
  ASSERT(4, sizeof(2147483647));
  ASSERT(4, sizeof(2147483648));
  ASSERT(-1, 0xfffffffff);
  ASSERT(4, sizeof(0xfffffffff));
  ASSERT(4, sizeof(4294967295U));
  ASSERT(4, sizeof(4294967296U));

  ASSERT(63, -1U>>30);
  ASSERT(3, -1Ul>>34);
  ASSERT(3, -1ull>>34);

  ASSERT(1, 0xfffffffffl>>35);
  ASSERT(1, 0xfffffffffll>>35);

  ASSERT(-1, 68719476735);
  ASSERT(4, sizeof(68719476735));
  ASSERT(1, 68719476735>>35);

  ASSERT(-1, 0xfffffffff);
  ASSERT(4, sizeof(0xfffffffff));
  ASSERT(1, 0xfffffffff>>35);

  ASSERT(-1, 0777777777777);
  ASSERT(4, sizeof(0777777777777));
  ASSERT(1, 0777777777777>>35);

  ASSERT(-1, 0b111111111111111111111111111111111111);
  ASSERT(4, sizeof(0b111111111111111111111111111111111111));
  ASSERT(1, 0b111111111111111111111111111111111111>>35);

  ASSERT(4, sizeof(2147483648));
  ASSERT(4, sizeof(2147483647));

  ASSERT(4, sizeof(0x1ffffffff));
  ASSERT(4, sizeof(0xffffffff));
  ASSERT(1, 0xffffffff>>31);

  ASSERT(4, sizeof(040000000000));
  ASSERT(4, sizeof(037777777777));
  ASSERT(1, 037777777777>>31);

  ASSERT(4, sizeof(0b111111111111111111111111111111111));
  ASSERT(4, sizeof(0b11111111111111111111111111111111));
  ASSERT(1, 0b11111111111111111111111111111111>>31);

  ASSERT(-1, 1 << 35 >> 35);
  ASSERT(-1, 01 << 35 >> 35);
  ASSERT(-1, 0x1 << 35 >> 35);
  ASSERT(-1, 0b1 << 35 >> 35);

  0.0;
  1.0;
  3e+8;
  0x10.1p0;
  .1E4f;

  ASSERT(4, sizeof(8f));

  assert(1, size\
of(char), \
         "sizeof(char)");

  ASSERT(4, sizeof(L'\0'));
  ASSERT(97, L'a');

  printf("OK\n");
  return 0;
}
