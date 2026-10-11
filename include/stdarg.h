#ifndef __STDARG_H
#define __STDARG_H

// ACR-7000: all arguments are passed on the stack, pushed right to left,
// so the variadic arguments simply follow the last named parameter in
// memory. Every argument occupies a whole number of words (four chars
// each), starting at the beginning of its first word.

typedef char *va_list;

#define __va_slot(n) (((n) + 3) & ~3)
#define va_start(ap, last) \
  ((ap) = (char *)((int *)&(last) + __va_slot(sizeof(last)) / 4))
#define va_arg(ap, ty) \
  (*(ty *)(((ap) += __va_slot(sizeof(ty))) - __va_slot(sizeof(ty))))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(ap) ((void)0)

#define __GNUC_VA_LIST 1
typedef va_list __gnuc_va_list;

#endif
