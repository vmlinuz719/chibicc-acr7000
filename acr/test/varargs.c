#include "test.h"  // ACR-7000 adaptation of test/varargs.c (see acr/test/adapt.py)
#include <stdarg.h>

int sum1(int x, ...) {
  va_list ap;
  va_start(ap, x);

  for (;;) {
    int y = va_arg(ap, int);
    if (y == 0)
      return x;
    x += y;
  }
}

typedef struct { int a, b, c; } S3;

// Structs are passed by value on the stack, so va_arg works with them.
int sum2(int x, ...) {
  va_list ap;
  va_start(ap, x);

  for (;;) {
    S3 y = va_arg(ap, S3);
    x += y.a + y.b + y.c;

    int z = va_arg(ap, int);
    if (z == 0)
      return x;
    x += z;
  }
}

void fmt(char *buf, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  va_list ap2;
  va_copy(ap2, ap);
  vsprintf(buf, fmt, ap2);
  va_end(buf);
}

int main() {
  ASSERT(6, sum1(1, 2, 3, 0));
  ASSERT(55, sum1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0));
  ASSERT(0, ({ char buf[100]; fmt(buf, "%d %d", 2, 3); strcmp(buf, "2 3"); }));

  ASSERT(21, sum2(1, (S3){1, 1, 0}, 3, (S3){2, 1, 1}, 5, (S3){3, 2, 1}, 0));
  ASSERT(0, ({ char buf[100]; fmt(buf, "%s %c %x %o %5d|%-3d|%03d", "a", 'b', 255, 8, 42, 1, 7); strcmp(buf, "a b ff 10    42|1  |007"); }));

  printf("OK\n");
  return 0;
}
