#ifndef __STDDEF_H
#define __STDDEF_H

#define NULL ((void *)0)

typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef unsigned int wchar_t;
typedef long max_align_t;

// Member addresses are in their own type's pointer representation, so
// measure the offset in chars.
#define offsetof(type, member) \
  ((size_t)((char *)&(((type *)0)->member) - (char *)0))

#endif
