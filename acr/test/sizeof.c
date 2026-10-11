#include "test.h"  // ACR-7000 adaptation of test/sizeof.c (see acr/test/adapt.py)

int main() {
  ASSERT(1, sizeof(char));
  ASSERT(1, sizeof(short));
  ASSERT(1, sizeof(short int));
  ASSERT(1, sizeof(int short));
  ASSERT(1, sizeof(int));
  ASSERT(1, sizeof(long));
  ASSERT(1, sizeof(long int));
  ASSERT(1, sizeof(long int));
  ASSERT(1, sizeof(char *));
  ASSERT(1, sizeof(int *));
  ASSERT(1, sizeof(long *));
  ASSERT(1, sizeof(int **));
  ASSERT(1, sizeof(int(*)[4]));
  ASSERT(4, sizeof(int*[4]));
  ASSERT(4, sizeof(int[4]));
  ASSERT(12, sizeof(int[3][4]));
  ASSERT(2, sizeof(struct {int a; int b;}));

  ASSERT(1, sizeof(-10 + (long)5));
  ASSERT(1, sizeof(-10 - (long)5));
  ASSERT(1, sizeof(-10 * (long)5));
  ASSERT(1, sizeof(-10 / (long)5));
  ASSERT(1, sizeof((long)-10 + 5));
  ASSERT(1, sizeof((long)-10 - 5));
  ASSERT(1, sizeof((long)-10 * 5));
  ASSERT(1, sizeof((long)-10 / 5));

  ASSERT(1, ({ char i; sizeof(++i); }));
  ASSERT(1, ({ char i; sizeof(i++); }));

  ASSERT(1, sizeof(int(*)[10]));
  ASSERT(1, sizeof(int(*)[][10]));

  ASSERT(1, sizeof(struct { int x, y[]; }));

  ASSERT(1, sizeof(char));
  ASSERT(1, sizeof(signed char));
  ASSERT(1, sizeof(signed char signed));
  ASSERT(1, sizeof(unsigned char));
  ASSERT(1, sizeof(unsigned char unsigned));

  ASSERT(1, sizeof(short));
  ASSERT(1, sizeof(int short));
  ASSERT(1, sizeof(short int));
  ASSERT(1, sizeof(signed short));
  ASSERT(1, sizeof(int short signed));
  ASSERT(1, sizeof(unsigned short));
  ASSERT(1, sizeof(int short unsigned));

  ASSERT(1, sizeof(int));
  ASSERT(1, sizeof(signed int));
  ASSERT(1, sizeof(signed));
  ASSERT(1, sizeof(signed signed));
  ASSERT(1, sizeof(unsigned int));
  ASSERT(1, sizeof(unsigned));
  ASSERT(1, sizeof(unsigned unsigned));

  ASSERT(1, sizeof(long));
  ASSERT(1, sizeof(signed long));
  ASSERT(1, sizeof(signed long int));
  ASSERT(1, sizeof(unsigned long));
  ASSERT(1, sizeof(unsigned long int));

  ASSERT(1, sizeof(long long));
  ASSERT(1, sizeof(signed long long));
  ASSERT(1, sizeof(signed long long int));
  ASSERT(1, sizeof(unsigned long long));
  ASSERT(1, sizeof(unsigned long long int));

  ASSERT(1, sizeof((char)1));
  ASSERT(1, sizeof((short)1));
  ASSERT(1, sizeof((int)1));
  ASSERT(1, sizeof((long)1));

  ASSERT(1, sizeof((char)1 + (char)1));
  ASSERT(1, sizeof((short)1 + (short)1));
  ASSERT(1, sizeof(1?2:3));
  ASSERT(1, sizeof(1?(short)2:(char)3));
  ASSERT(1, sizeof(1?(long)2:(char)3));

  ASSERT(1, sizeof(char) << 31 >> 31);
  ASSERT(1, sizeof(char) << 35 >> 35);


  ASSERT(1, sizeof(1f+2));
  ASSERT(1, sizeof(1f-2));
  ASSERT(1, sizeof(1f*2));
  ASSERT(1, sizeof(1f/2));


  ASSERT(1, sizeof(main));

  printf("OK\n");
  return 0;
}
