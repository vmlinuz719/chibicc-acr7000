#ifndef __STDIO_H
#define __STDIO_H

#include <stddef.h>
#include <stdarg.h>

// ACR-7000: stdout and stderr go to the line printer (device 013),
// stdin comes from the paper tape reader (device 012).

typedef struct __FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)
#define BUFSIZ 132

int putchar(int c);
int putc(int c, FILE *fp);
int fputc(int c, FILE *fp);
int puts(const char *s);
int fputs(const char *s, FILE *fp);
size_t fwrite(const void *ptr, size_t size, size_t n, FILE *fp);
int fflush(FILE *fp);

int getchar(void);
int getc(FILE *fp);
int fgetc(FILE *fp);
char *fgets(char *buf, int n, FILE *fp);
int ungetc(int c, FILE *fp);

int printf(const char *fmt, ...);
int fprintf(FILE *fp, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *fp, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

void perror(const char *s);

#endif
