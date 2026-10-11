#ifndef __SETJMP_H
#define __SETJMP_H

// Return address, caller's AP, caller's SP.
typedef int jmp_buf[3];

int setjmp(jmp_buf env);
_Noreturn void longjmp(jmp_buf env, int val);

#endif
