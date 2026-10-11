#ifndef __STDARG_H
#define __STDARG_H

// ACR-7000: all arguments are passed on the stack, pushed right to left,
// so the variadic arguments simply follow the last named parameter in
// memory. Every scalar argument takes one word.

typedef char *va_list;

#define va_start(ap, last) ((ap) = (char *)(&(last) + 1))
#define va_arg(ap, ty) (*(ty *)(((ap) += sizeof(ty)) - sizeof(ty)))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(ap) ((void)0)

#define __GNUC_VA_LIST 1
typedef va_list __gnuc_va_list;

#endif
