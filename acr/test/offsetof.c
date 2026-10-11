#include "test.h"  // ACR-7000 adaptation of test/offsetof.c (see acr/test/adapt.py)
#include <stddef.h>

typedef struct {
  int a;
  char b;
  int c;
  long d;
} T;

int main() {
  ASSERT(0, offsetof(T, a));
  ASSERT(1, offsetof(T, b));
  ASSERT(2, offsetof(T, c));
  ASSERT(3, offsetof(T, d));

  printf("OK\n");
  return 0;
}
