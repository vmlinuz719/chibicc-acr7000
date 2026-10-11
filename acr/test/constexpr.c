#include "test.h"  // ACR-7000 adaptation of test/constexpr.c (see acr/test/adapt.py)


int main() {
  ASSERT(10, ({ enum { ten=1+2+3+4 }; ten; }));
  ASSERT(1, ({ int i=0; switch(3) { case 5-2+0*3: i++; } i; }));
  ASSERT(2, ({ int x[1+1]; sizeof(x); }));
  ASSERT(6, ({ char x[8-2]; sizeof(x); }));
  ASSERT(6, ({ char x[2*3]; sizeof(x); }));
  ASSERT(3, ({ char x[12/4]; sizeof(x); }));
  ASSERT(2, ({ char x[12%10]; sizeof(x); }));
  ASSERT(0b100, ({ char x[0b110&0b101]; sizeof(x); }));
  ASSERT(0b111, ({ char x[0b110|0b101]; sizeof(x); }));
  ASSERT(0b110, ({ char x[0b111^0b001]; sizeof(x); }));
  ASSERT(4, ({ char x[1<<2]; sizeof(x); }));
  ASSERT(2, ({ char x[4>>1]; sizeof(x); }));
  ASSERT(2, ({ char x[(1==1)+1]; sizeof(x); }));
  ASSERT(1, ({ char x[(1!=1)+1]; sizeof(x); }));
  ASSERT(1, ({ char x[(1<1)+1]; sizeof(x); }));
  ASSERT(2, ({ char x[(1<=1)+1]; sizeof(x); }));
  ASSERT(2, ({ char x[1?2:3]; sizeof(x); }));
  ASSERT(3, ({ char x[0?2:3]; sizeof(x); }));
  ASSERT(3, ({ char x[(1,3)]; sizeof(x); }));
  ASSERT(2, ({ char x[!0+1]; sizeof(x); }));
  ASSERT(1, ({ char x[!1+1]; sizeof(x); }));
  ASSERT(2, ({ char x[~-3]; sizeof(x); }));
  ASSERT(2, ({ char x[(5||6)+1]; sizeof(x); }));
  ASSERT(1, ({ char x[(0||0)+1]; sizeof(x); }));
  ASSERT(2, ({ char x[(1&&1)+1]; sizeof(x); }));
  ASSERT(1, ({ char x[(1&&0)+1]; sizeof(x); }));
  ASSERT(3, ({ char x[(int)3]; sizeof(x); }));
  ASSERT(4, ({ char x[(int)0xfffffffff+5]; sizeof(x); }));
  ASSERT(2, ({ char x[(int*)0+2]; sizeof(x); }));
  ASSERT(15, ({ char x[(int*)16-1]; sizeof(x); }));
  ASSERT(12, ({ char x[(int*)16-(int*)4]; sizeof(x); }));

  ASSERT(4, ({ char x[(-1>>31)+5]; sizeof(x); }));
  ASSERT(31, ({ char x[(unsigned int)0xfffffffff>>31]; sizeof(x); }));
  ASSERT(1, ({ char x[(long)-1/((long)1<<34)+1]; sizeof(x); }));
  ASSERT(4, ({ char x[(unsigned long)-1/((long)1<<34)+1]; sizeof(x); }));
  ASSERT(1, ({ char x[(unsigned)1<-1]; sizeof(x); }));
  ASSERT(1, ({ char x[(unsigned)1<=-1]; sizeof(x); }));


  printf("OK\n");
  return 0;
}
