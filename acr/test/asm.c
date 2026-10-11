#include "test.h"  // ACR-7000 adaptation of test/asm.c (see acr/test/adapt.py)

char *asm_fn1(void) {
  asm("            ldea        ac, 50\n"
      "            movr        ap, sp\n"
      "            rets");
}

char *asm_fn2(void) {
  asm inline volatile("            ldea        ac, 55\n"
                      "            movr        ap, sp\n"
                      "            rets");
}

int main() {
  ASSERT(50, asm_fn1());
  ASSERT(55, asm_fn2());

  printf("OK\n");
  return 0;
}
