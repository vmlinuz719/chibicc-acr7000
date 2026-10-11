#include "test.h"  // ACR-7000 adaptation of test/tls.c (see acr/test/adapt.py)
#include <stdio.h>

_Thread_local int v1;
_Thread_local int v2 = 5;
int v3 = 7;

int thread_main(void *unused) {
  ASSERT(0, v1);
  ASSERT(5, v2);
  ASSERT(7, v3);

  v1 = 1;
  v2 = 2;
  v3 = 3;

  ASSERT(1, v1);
  ASSERT(2, v2);
  ASSERT(3, v3);

  return 0;
}

int main() {

  ASSERT(0, v1);
  ASSERT(5, v2);
  ASSERT(7, v3);

  ASSERT(0, thread_main(NULL));

  // a single thread: no separate copies
  ASSERT(1, v1);
  ASSERT(2, v2);
  ASSERT(3, v3);

  printf("OK\n");
  return 0;
}
