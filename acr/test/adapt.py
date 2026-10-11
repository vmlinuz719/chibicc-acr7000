#!/usr/bin/env python3
# Generates the ACR-7000 adaptations of chibicc's tests (acr/test/*.c)
# from test/*.c. Each change is listed explicitly; expectations were
# recomputed by hand for 36-bit words with sizeof(int) == 1.
import os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(os.path.dirname(here))

FLOAT = re.compile(r'\bfloat\b|\bdouble\b|(?<![\w.])\d+\.\d*(?:[eE][+-]?\d+)?[fFlL]?'
                   r'|(?<![\w.])\.\d+|(?<![\w])\d+[eE][+-]?\d+|0x[0-9a-fA-F.]*p[+-]?\d+')

DEL = None

NOTE = '  // ACR-7000 adaptation of test/%s (see acr/test/adapt.py)'
done = set()

def adapt(name, edits=(), drop_float=True, text_edits=()):
    # A second call for the same file applies further edits.
    if name in done:
        src = open(os.path.join(here, name)).read().replace(NOTE % name, '', 1)
    else:
        src = open(os.path.join(root, 'test', name)).read()
    done.add(name)
    for old, new in text_edits:
        assert old in src, (name, old)
        src = src.replace(old, new)
    out = []
    used = set()
    for line in src.split('\n'):
        s = line.strip()
        hit = None
        for i, (pat, new) in enumerate(edits):
            if pat in line:
                hit = i
                break
        if hit is not None:
            used.add(hit)
            new = edits[hit][1]
            if new is not DEL:
                out.append(line[:len(line) - len(line.lstrip())] + new)
            continue
        if drop_float and FLOAT.search(line) and s.startswith('ASSERT(') and s.endswith(');'):
            continue
        out.append(line)
    for i, (pat, new) in enumerate(edits):
        assert i in used, (name, pat)
    # Keep line numbers intact: put the note at the end of the first line.
    out[0] += NOTE % name
    open(os.path.join(here, name), 'w').write('\n'.join(out))

adapt('function.c', [
    # x86-64 va_list layout replaced with <stdarg.h>
    ('typedef __va_elem va_list[1];', 'typedef char *va_list;'),
    ('*ap = *(__va_elem *)__va_area__;', 'ap = (char *)(&fmt + 1);'),
    ('typedef struct { int a; float b; double c; } Ty5;', 'typedef struct { int a; int b; int c; } Ty5;'),
    ('int struct_test5(Ty5 x, int n);', DEL),
    ('Ty5 struct_test25(void);', DEL),
    ('struct_test5(x, 0)', DEL), ('struct_test5(x, 1)', DEL), ('struct_test5(x, 2)', DEL),
    ('struct_test25().a', DEL), ('struct_test25().b', DEL), ('struct_test25().c', DEL),
    ('add10_float(', DEL), ('add10_double(', DEL),
    ('add_float(', DEL), ('add_double(', DEL),
    ('add_float3(', DEL), ('add_double3(', DEL),
    ('many_args2(1,2,3,4,5,6,7,8,40,10)', DEL),
    ('many_args3(1,2,3,4,5,6,7,8,9,10,11,12,13,14,80,10)', DEL),
], text_edits=[
    ('''float add_float3(float x, float y, float z) {
  return x + y + z;
}

double add_double3(double x, double y, double z) {
  return x + y + z;
}
''', ''),
    ('''double to_double(long double x) {
  return x;
}

long double to_ldouble(int x) {
  return x;
}
''', ''),
    ('''double many_args2(double a, double b, double c, double d, double e,
                  double f, double g, double h, double i, double j) {
  return i / j;
}

int many_args3(int a, double b, int c, int d, double e, int f,
               double g, int h, double i, double j, double k,
               double l, double m, int n, int o, double p) {
  return o / p;
}
''', ''),
])


adapt('macro.c', [
    ('strcmp(main_filename1, "test/macro.c")', 'ASSERT(0, strcmp(main_filename1, "acr/test/macro.c"));'),
    ('strcmp(__BASE_FILE__, "test/macro.c")', 'ASSERT(0, strcmp(__BASE_FILE__, "acr/test/macro.c"));'),
])


VARARGS_SUM2 = r'''typedef struct { int a, b, c; } S3;

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
'''

VARARGS_EXTRA = r'''  ASSERT(21, sum2(1, (S3){1, 1, 0}, 3, (S3){2, 1, 1}, 5, (S3){3, 2, 1}, 0));
  ASSERT(0, ({ char buf[100]; fmt(buf, "%s %c %x %o %5d|%-3d|%03d", "a", 'b', 255, 8, 42, 1, 7); strcmp(buf, "a b ff 10    42|1  |007"); }));

  printf("OK\n");'''

adapt('varargs.c', text_edits=[('''int sum2(int x, ...) {
  va_list ap;
  va_start(ap, x);

  for (;;) {
    double y = va_arg(ap, double);
    x += y;

    int z = va_arg(ap, int);
    if (z == 0)
      return x;
    x += z;
  }
}
''', VARARGS_SUM2), ('  printf("OK\\n");', VARARGS_EXTRA)])


# --- x86-64 inline assembly rewritten for the ACR-7000 -------------------

adapt('asm.c', text_edits=[
    ('''  asm("mov $50, %rax\\n\\t"
      "mov %rbp, %rsp\\n\\t"
      "pop %rbp\\n\\t"
      "ret");''',
     '''  asm("            ldea        ac, 50\\n"
      "            movr        ap, sp\\n"
      "            rets");'''),
    ('''  asm inline volatile("mov $55, %rax\\n\\t"
                      "mov %rbp, %rsp\\n\\t"
                      "pop %rbp\\n\\t"
                      "ret");''',
     '''  asm inline volatile("            ldea        ac, 55\\n"
                      "            movr        ap, sp\\n"
                      "            rets");'''),
])

# --- no threads: run the thread functions one after another ------------

adapt('atomic.c', text_edits=[
    ('#include <pthread.h>\n', ''),
    ('1000*1000', '1000'),
    ('''  pthread_t thr1;
  pthread_t thr2;
  pthread_t thr3;

  pthread_create(&thr1, NULL, add1, &x);
  pthread_create(&thr2, NULL, add2, &x);
  pthread_create(&thr3, NULL, add3, &x);
''', '''  add1(&x);
  add2(&x);
  add3(&x);
'''),
    ('''  pthread_join(thr1, NULL);
  pthread_join(thr2, NULL);
  pthread_join(thr3, NULL);
''', ''),
])

adapt('tls.c', text_edits=[
    ('#include <pthread.h>\n', ''),
    ('''  pthread_t thr;
''', ''),
    ('''  ASSERT(0, pthread_create(&thr, NULL, thread_main, NULL));
  ASSERT(0, pthread_join(thr, NULL));

  ASSERT(0, v1);
  ASSERT(5, v2);
  ASSERT(3, v3);''', '''  ASSERT(0, thread_main(NULL));

  // a single thread: no separate copies
  ASSERT(1, v1);
  ASSERT(2, v2);
  ASSERT(3, v3);'''),
])

# _Generic does not evaluate its controlling expression, so the
# floating-point cases need no floating-point code.
adapt('generic.c', drop_float=False)

# --- expectations for 9-bit chars, 18-bit shorts and 36-bit ints --------

# long is one word; structs are word-aligned; locals each start a word; x86-64 ABI array alignment and over-alignment of variables do not apply
adapt('alignof.c', [
    (', _Alignof(long));', 'ASSERT(4, _Alignof(long));'),
    (', _Alignof(long long));', 'ASSERT(4, _Alignof(long long));'),
    (', _Alignof(struct {char a; char b;}[2]));', 'ASSERT(4, _Alignof(struct {char a; char b;}[2]));'),
    (', _Alignof(struct {char a; long b;}[2]));', 'ASSERT(4, _Alignof(struct {char a; long b;}[2]));'),
    (', ({ _Alignas(char) char x, y; &y-&x; }));', 'ASSERT(4, ({ _Alignas(char) char x, y; &y-&x; }));'),
    (', ({ _Alignas(long) char x, y; &y-&x; }));', 'ASSERT(4, ({ _Alignas(long) char x, y; &y-&x; }));'),
    (', ({ _Alignas(32) char x, y; &y-&x; }));', DEL),
    (', ({ _Alignas(32) int *x, *y; ((char *)&y)-((char *)&x); }));', DEL),
    (', (long)(char *)&g1 % 512);', DEL),
    (', (long)(char *)&g2 % 512);', DEL),
    (', (long)(char *)&g5 % 8);', 'ASSERT(0, (long)(char *)&g5 % 4);'),
    (', _Alignof(char) << 63 >> 63);', 'ASSERT(1, _Alignof(char) << 35 >> 35);'),
    (', ({ char x; _Alignof(x) << 63 >> 63; }));', 'ASSERT(1, ({ char x; _Alignof(x) << 35 >> 35; }));'),
    (', ({ char x[16]; (unsigned long)&x % 16; }));', DEL),
    (', ({ char x[17]; (unsigned long)&x % 16; }));', DEL),
    (', ({ char x[101]; (unsigned long)&x % 16; }));', DEL),
    (', ({ char x[100]; (unsigned long)&x % 16; }));', DEL),
], text_edits=[('int _Alignas(512) g1;\nint _Alignas(512) g2;\n', '')])

# alloca rounds sizes up to whole words
adapt('alloca.c', [
    (', p2 - p3);', 'ASSERT(4, p2 - p3);'),
])

# 36-bit int arithmetic; long is one word
adapt('arith.c', [
    (', 1073741824 * 100 / 100);', 'ASSERT(-300647710, 1073741824 * 100 / 100);'),
    (', sizeof(0?(long)1:(long)2));', 'ASSERT(4, sizeof(0?(long)1:(long)2));'),
])

# packed is ignored (an int cannot be accessed at a char offset), so these structs get normal word-aligned layouts
adapt('attribute.c', [
    (', ({ struct { char a; int b; } __attribute__((packed)) x; sizeof(x); }));', 'ASSERT(8, ({ struct { char a; int b; } __attribute__((packed)) x; sizeof(x); }));'),
    (', offsetof(struct __attribute__((packed)) { char a; int b; }, b));', 'ASSERT(4, offsetof(struct __attribute__((packed)) { char a; int b; }, b));'),
    (', ({ struct __attribute__((packed)) { char a; int b; } x; sizeof(x); }));', 'ASSERT(8, ({ struct __attribute__((packed)) { char a; int b; } x; sizeof(x); }));'),
    (', offsetof(struct { char a; int b; } __attribute__((packed)), b));', 'ASSERT(4, offsetof(struct { char a; int b; } __attribute__((packed)), b));'),
    (', ({ typedef struct { char a; int b[2]; } __attribute__((packed)) T; sizeof(T); }));', 'ASSERT(12, ({ typedef struct { char a; int b[2]; } __attribute__((packed)) T; sizeof(T); }));'),
    (', ({ typedef struct __attribute__((packed)) { char a; int b[2]; } T; sizeof(T); }));', 'ASSERT(12, ({ typedef struct __attribute__((packed)) { char a; int b[2]; } T; sizeof(T); }));'),
    (', offsetof(struct __attribute__((packed)) T { char a; int b[2]; }, b));', 'ASSERT(4, offsetof(struct __attribute__((packed)) T { char a; int b[2]; }, b));'),
    (', _Alignof(struct __attribute__((packed)) { char a; int b[2]; }));', 'ASSERT(4, _Alignof(struct __attribute__((packed)) { char a; int b[2]; }));'),
    (', offsetof(struct __attribute__((aligned(8), packed)) { char a; int b; }, b));', 'ASSERT(4, offsetof(struct __attribute__((aligned(8), packed)) { char a; int b; }, b));'),
    (', offsetof(struct { char a; int b; } __attribute__((aligned(8), packed)), b));', 'ASSERT(4, offsetof(struct { char a; int b; } __attribute__((aligned(8), packed)), b));'),
    (', offsetof(struct __attribute__((aligned(8))) __attribute__((packed)) { char a; int b; }, b));', 'ASSERT(4, offsetof(struct __attribute__((aligned(8))) __attribute__((packed)) { char a; int b; }, b));'),
    (', offsetof(struct { char a; int b; } __attribute__((aligned(8))) __attribute__((packed)), b));', 'ASSERT(4, offsetof(struct { char a; int b; } __attribute__((aligned(8))) __attribute__((packed)), b));'),
    (', offsetof(struct __attribute__((aligned(8))) { char a; int b; } __attribute__((packed)), b));', 'ASSERT(4, offsetof(struct __attribute__((aligned(8))) { char a; int b; } __attribute__((packed)), b));'),
])

# long is one word
adapt('bitfield.c', [
    (', sizeof(struct {long x:1; }));', 'ASSERT(4, sizeof(struct {long x:1; }));'),
])

# 9-bit chars (plain char unsigned), 18-bit shorts, 36-bit ints and longs, big-endian packing
adapt('cast.c', [
    (', (int)8590066177);', 'ASSERT(8590066177, (int)8590066177);'),
    (', (short)8590066177);', 'ASSERT(-130559, (short)8590066177);'),
    (', ({ int x=512; *(char *)&x=1; x; }));', 'ASSERT(134218240, ({ int x=512; *(char *)&x=1; x; }));'),
    (', (char)255);', 'ASSERT(255, (char)255);'),
    (', (signed char)255);', 'ASSERT(255, (signed char)255);'),
    (', (short)65535);', 'ASSERT(65535, (short)65535);'),
    (', (int)0xffffffff);', 'ASSERT(4294967295, (int)0xffffffff);'),
    (', ((unsigned)-1)>>1);', 'ASSERT(34359738367, ((unsigned)-1)>>1);'),
    (', ((unsigned)-100)/2);', 'ASSERT(34359738318, ((unsigned)-100)/2);'),
    (', ((unsigned long)-100)/2);', 'ASSERT(34359738318, ((unsigned long)-100)/2);'),
    (', ((long)-1)/(unsigned)100);', 'ASSERT(687194767, ((long)-1)/(unsigned)100);'),
    (', ((unsigned)-100)%7);', 'ASSERT(6, ((unsigned)-100)%7);'),
    (', ((unsigned long)-100)%9);', 'ASSERT(0, ((unsigned long)-100)%9);'),
    (', ({ typedef short T; T x = 65535; (int)x; }));', 'ASSERT(65535, ({ typedef short T; T x = 65535; (int)x; }));'),
])

# 0xffffffff is not -1 as a 36-bit int
adapt('control.c', [
    (', ({ int i=0; switch(-1) { case 0xffffffff: i=3; break; } i; }));', 'ASSERT(0, ({ int i=0; switch(-1) { case 0xffffffff: i=3; break; } i; }));'),
])

# long and long long are one word; (char)256 is 256
adapt('decl.c', [
    (', ({ long int x; sizeof(x); }));', 'ASSERT(4, ({ long int x; sizeof(x); }));'),
    (', ({ int long x; sizeof(x); }));', 'ASSERT(4, ({ int long x; sizeof(x); }));'),
    (', ({ long long x; sizeof(x); }));', 'ASSERT(4, ({ long long x; sizeof(x); }));'),
    (', (_Bool)(char)256);', 'ASSERT(1, (_Bool)(char)256);'),
])

# char and short are 9 and 18 bits; the *_fn helpers are defined as returning int but declared returning narrower types, and the ACR-7000 ABI does not narrow return values at the call
adapt('function.c', [
    (', int_to_char(261));', 'ASSERT(261, int_to_char(261));'),
    (', true_fn());', 'ASSERT(513, true_fn());'),
    (', false_fn());', 'ASSERT(512, false_fn());'),
    (', char_fn());', 'ASSERT(515, char_fn());'),
    (', short_fn());', 'ASSERT(131077, short_fn());'),
    (', uchar_fn());', 'ASSERT(2043, uchar_fn());'),
    (', ushort_fn());', 'ASSERT(2097144, ushort_fn());'),
    (', schar_fn());', 'ASSERT(2043, schar_fn());'),
    (', sshort_fn());', 'ASSERT(2097144, sshort_fn());'),
])

# big-endian packing of 9-bit chars; structs are padded to whole words
adapt('initializer.c', [
    (', ({ union { int a; char b[4]; } x={0x01020304}; x.b[0]; }));', 'ASSERT(0, ({ union { int a; char b[4]; } x={0x01020304}; x.b[0]; }));'),
    (', ({ union { int a; char b[4]; } x={0x01020304}; x.b[1]; }));', 'ASSERT(64, ({ union { int a; char b[4]; } x={0x01020304}; x.b[1]; }));'),
    (', ({ union { struct { char a,b,c,d; } e; int f; } x={{4,3,2,1}}; x.f; }));', 'ASSERT(537658369, ({ union { struct { char a,b,c,d; } e; int f; } x={{4,3,2,1}}; x.f; }));'),
    (', g13[0].b[0]);', 'ASSERT(0, g13[0].b[0]);'),
    (', g13[0].b[1]);', 'ASSERT(64, g13[0].b[1]);'),
    (', g13[1].b[0]);', 'ASSERT(0, g13[1].b[0]);'),
    (', g13[1].b[1]);', 'ASSERT(321, g13[1].b[1]);'),
    (', *g28);', 'ASSERT(402653184, *g28);'),
    (', sizeof(g65));', 'ASSERT(7, sizeof(g65));'),
    (', sizeof(g66));', 'ASSERT(10, sizeof(g66));'),
    (', ({ union { unsigned short a; char b[2]; } x={.b[0]=0xff}; x.a; }));', 'ASSERT(130560, ({ union { unsigned short a; char b[2]; } x={.b[0]=0xff}; x.a; }));'),
    (', ({ union { unsigned short a; char b[2]; } x={.b[1]=0xff}; x.a; }));', 'ASSERT(255, ({ union { unsigned short a; char b[2]; } x={.b[1]=0xff}; x.a; }));'),
    (', g50.a);', 'ASSERT(9216, g50.a);'),
])

# 36-bit int and long; plain char is unsigned; shifts beyond 35 bits rewritten
adapt('literal.c', [
    (", '\\x80');", "ASSERT(128, '\\x80');"),
    (', sizeof(0L));', 'ASSERT(4, sizeof(0L));'),
    (', sizeof(0LU));', 'ASSERT(4, sizeof(0LU));'),
    (', sizeof(0UL));', 'ASSERT(4, sizeof(0UL));'),
    (', sizeof(0LL));', 'ASSERT(4, sizeof(0LL));'),
    (', sizeof(0LLU));', 'ASSERT(4, sizeof(0LLU));'),
    (', sizeof(0Ull));', 'ASSERT(4, sizeof(0Ull));'),
    (', sizeof(0l));', 'ASSERT(4, sizeof(0l));'),
    (', sizeof(0ll));', 'ASSERT(4, sizeof(0ll));'),
    (', sizeof(0x0L));', 'ASSERT(4, sizeof(0x0L));'),
    (', sizeof(0b0L));', 'ASSERT(4, sizeof(0b0L));'),
    (', sizeof(2147483648));', 'ASSERT(4, sizeof(2147483648));'),
    (', sizeof(0xffffffffffffffff));', 'ASSERT(4, sizeof(0xfffffffff));'),
    (', sizeof(4294967296U));', 'ASSERT(4, sizeof(4294967296U));'),
    (', -1U>>30);', 'ASSERT(63, -1U>>30);'),
    (', -1Ul>>62);', 'ASSERT(3, -1Ul>>34);'),
    (', -1ull>>62);', 'ASSERT(3, -1ull>>34);'),
    (', 0xffffffffffffffffl>>63);', 'ASSERT(1, 0xfffffffffl>>35);'),
    (', 0xffffffffffffffffll>>63);', 'ASSERT(1, 0xfffffffffll>>35);'),
    (', sizeof(18446744073709551615));', 'ASSERT(4, sizeof(68719476735));'),
    (', 18446744073709551615>>63);', 'ASSERT(1, 68719476735>>35);'),
    (', 0xffffffffffffffff>>63);', 'ASSERT(1, 0xfffffffff>>35);'),
    (', sizeof(01777777777777777777777));', 'ASSERT(4, sizeof(0777777777777));'),
    (', 01777777777777777777777>>63);', 'ASSERT(1, 0777777777777>>35);'),
    (', sizeof(0b1111111111111111111111111111111111111111111111111111111111111111));', 'ASSERT(4, sizeof(0b111111111111111111111111111111111111));'),
    (', 0b1111111111111111111111111111111111111111111111111111111111111111>>63);', 'ASSERT(1, 0b111111111111111111111111111111111111>>35);'),
    (', sizeof(0x1ffffffff));', 'ASSERT(4, sizeof(0x1ffffffff));'),
    (', sizeof(040000000000));', 'ASSERT(4, sizeof(040000000000));'),
    (', sizeof(0b111111111111111111111111111111111));', 'ASSERT(4, sizeof(0b111111111111111111111111111111111));'),
    (', 1 << 31 >> 31);', 'ASSERT(-1, 1 << 35 >> 35);'),
    (', 01 << 31 >> 31);', 'ASSERT(-1, 01 << 35 >> 35);'),
    (', 0x1 << 31 >> 31);', 'ASSERT(-1, 0x1 << 35 >> 35);'),
    (', 0b1 << 31 >> 31);', 'ASSERT(-1, 0b1 << 35 >> 35);'),
    (', 0xffffffffffffffff);', 'ASSERT(-1, 0xfffffffff);'),
    (', 18446744073709551615);', 'ASSERT(-1, 68719476735);'),
    (', 01777777777777777777777);', 'ASSERT(-1, 0777777777777);'),
    (', 0b1111111111111111111111111111111111111111111111111111111111111111);', 'ASSERT(-1, 0b111111111111111111111111111111111111);'),
])

# double is word-aligned
adapt('offsetof.c', [
    (', offsetof(T, d));', 'ASSERT(12, offsetof(T, d));'),
])

# long, long long and pointers are one word (4 chars)
adapt('sizeof.c', [
    (', sizeof(long));', 'ASSERT(4, sizeof(long));'),
    (', sizeof(long int));', 'ASSERT(4, sizeof(long int));'),
    (', sizeof(char *));', 'ASSERT(4, sizeof(char *));'),
    (', sizeof(int *));', 'ASSERT(4, sizeof(int *));'),
    (', sizeof(long *));', 'ASSERT(4, sizeof(long *));'),
    (', sizeof(int **));', 'ASSERT(4, sizeof(int **));'),
    (', sizeof(int(*)[4]));', 'ASSERT(4, sizeof(int(*)[4]));'),
    (', sizeof(int*[4]));', 'ASSERT(16, sizeof(int*[4]));'),
    (', sizeof(-10 + (long)5));', 'ASSERT(4, sizeof(-10 + (long)5));'),
    (', sizeof(-10 - (long)5));', 'ASSERT(4, sizeof(-10 - (long)5));'),
    (', sizeof(-10 * (long)5));', 'ASSERT(4, sizeof(-10 * (long)5));'),
    (', sizeof(-10 / (long)5));', 'ASSERT(4, sizeof(-10 / (long)5));'),
    (', sizeof((long)-10 + 5));', 'ASSERT(4, sizeof((long)-10 + 5));'),
    (', sizeof((long)-10 - 5));', 'ASSERT(4, sizeof((long)-10 - 5));'),
    (', sizeof((long)-10 * 5));', 'ASSERT(4, sizeof((long)-10 * 5));'),
    (', sizeof((long)-10 / 5));', 'ASSERT(4, sizeof((long)-10 / 5));'),
    (', sizeof(int(*)[10]));', 'ASSERT(4, sizeof(int(*)[10]));'),
    (', sizeof(int(*)[][10]));', 'ASSERT(4, sizeof(int(*)[][10]));'),
    (', sizeof(signed long));', 'ASSERT(4, sizeof(signed long));'),
    (', sizeof(signed long int));', 'ASSERT(4, sizeof(signed long int));'),
    (', sizeof(unsigned long));', 'ASSERT(4, sizeof(unsigned long));'),
    (', sizeof(unsigned long int));', 'ASSERT(4, sizeof(unsigned long int));'),
    (', sizeof(long long));', 'ASSERT(4, sizeof(long long));'),
    (', sizeof(signed long long));', 'ASSERT(4, sizeof(signed long long));'),
    (', sizeof(signed long long int));', 'ASSERT(4, sizeof(signed long long int));'),
    (', sizeof(unsigned long long));', 'ASSERT(4, sizeof(unsigned long long));'),
    (', sizeof(unsigned long long int));', 'ASSERT(4, sizeof(unsigned long long int));'),
    (', sizeof((long)1));', 'ASSERT(4, sizeof((long)1));'),
    (', sizeof(1?(long)2:(char)3));', 'ASSERT(4, sizeof(1?(long)2:(char)3));'),
    (', sizeof(char) << 63 >> 63);', 'ASSERT(1, sizeof(char) << 35 >> 35);'),
])

# structs are word-aligned and padded to whole words; long is one word
adapt('struct.c', [
    (', ({ struct {char a; char b;} x[3]; char *p=x; p[2]=2; x[1].a; }));', 'ASSERT(2, ({ struct {char a; char b;} x[3]; char *p=x; p[4]=2; x[1].a; }));'),
    (', ({ struct {char a; char b;} x[3]; char *p=x; p[3]=3; x[1].b; }));', 'ASSERT(3, ({ struct {char a; char b;} x[3]; char *p=x; p[5]=3; x[1].b; }));'),
    (', ({ struct {char a; char b;} x; sizeof(x); }));', 'ASSERT(4, ({ struct {char a; char b;} x; sizeof(x); }));'),
    (', ({ struct t {char a[2];}; { struct t {char a[4];}; } struct t y; sizeof(y); }));', 'ASSERT(4, ({ struct t {char a[2];}; { struct t {char a[4];}; } struct t y; sizeof(y); }));'),
    (', ({ struct {char a; long b;} x; sizeof(x); }));', 'ASSERT(8, ({ struct {char a; long b;} x; sizeof(x); }));'),
    (', ({ struct foo *bar; sizeof(bar); }));', 'ASSERT(4, ({ struct foo *bar; sizeof(bar); }));'),
])

# pointers are 4 chars
adapt('typeof.c', [
    (', ({ int x; typeof(&x) y; sizeof(y); }));', 'ASSERT(4, ({ int x; typeof(&x) y; sizeof(y); }));'),
])

# wchar_t is a 36-bit int; byte-level comparisons of wide strings replaced by element comparisons
adapt('unicode.c', [
    (", L'\\xffffffff'>>31);", "ASSERT(1, L'\\xffffffff'>>31);"),
    (', memcmp(u"abc", "a\\0b\\0c\\0\\0\\0", 8));', 'ASSERT(0, memcmp(u"abc", (char16_t[]){97, 98, 99, 0}, 8));'),
    (', memcmp(u"日本語", "\\345e,g\\236\\212\\0\\0", 8));', 'ASSERT(0, memcmp(u"日本語", (char16_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 8));'),
    (', memcmp(u"🍣", "<\\330c\\337\\0\\0", 6));', 'ASSERT(0, memcmp(u"🍣", (char16_t[]){0xd83c, 0xdf63, 0}, 6));'),
    (', memcmp(U"abc", "a\\0\\0\\0b\\0\\0\\0c\\0\\0\\0\\0\\0\\0\\0", 16));', 'ASSERT(0, memcmp(U"abc", (char32_t[]){97, 98, 99, 0}, 16));'),
    (', memcmp(U"日本語", "\\345e\\0\\0,g\\0\\0\\236\\212\\0\\0\\0\\0\\0\\0", 16));', 'ASSERT(0, memcmp(U"日本語", (char32_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 16));'),
    (', memcmp(U"🍣", "c\\363\\001\\0\\0\\0\\0\\0", 8));', 'ASSERT(0, memcmp(U"🍣", (char32_t[]){0x1f363, 0}, 8));'),
    (', memcmp(L"abc", "a\\0\\0\\0b\\0\\0\\0c\\0\\0\\0\\0\\0\\0\\0", 16));', 'ASSERT(0, memcmp(L"abc", (wchar_t[]){97, 98, 99, 0}, 16));'),
    (', memcmp(L"日本語", "\\345e\\0\\0,g\\0\\0\\236\\212\\0\\0\\0\\0\\0\\0", 16));', 'ASSERT(0, memcmp(L"日本語", (wchar_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 16));'),
    (', memcmp(L"🍣", "c\\363\\001\\0\\0\\0\\0\\0", 8));', 'ASSERT(0, memcmp(L"🍣", (wchar_t[]){0x1f363, 0}, 8));'),
    (', L"\\xffffffff"[0] >> 31);', 'ASSERT(1, L"\\xffffffff"[0] >> 31);'),
])

# big-endian packing of 9-bit chars
adapt('union.c', [
    (', ({ union { int a; char b[4]; } x; x.a = 515; x.b[0]; }));', 'ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[0]; }));'),
    (', ({ union { int a; char b[4]; } x; x.a = 515; x.b[1]; }));', 'ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[1]; }));'),
    (', ({ union { int a; char b[4]; } x; x.a = 515; x.b[2]; }));', 'ASSERT(1, ({ union { int a; char b[4]; } x; x.a = 515; x.b[2]; }));'),
    (', ({ union { int a; char b[4]; } x; x.a = 515; x.b[3]; }));', 'ASSERT(3, ({ union { int a; char b[4]; } x; x.a = 515; x.b[3]; }));'),
    (', ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.a; }));', 'ASSERT(27, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.a; }));'),
    (', ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.b; }));', 'ASSERT(427, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.b; }));'),
    (', ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.c; }));', 'ASSERT(223, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.c; }));'),
    (', ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.d; }));', 'ASSERT(239, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.d; }));'),
])

# no overflow in 36 bits
adapt('usualconv.c', [
    (', 2147483647 + 2147483647 + 2);', 'ASSERT(4294967296, 2147483647 + 2147483647 + 2);'),
])

# pointers and long are 4 chars
adapt('variable.c', [
    (', ({ int *x; sizeof(x); }));', 'ASSERT(4, ({ int *x; sizeof(x); }));'),
    (', ({ long x; sizeof(x); }));', 'ASSERT(4, ({ long x; sizeof(x); }));'),
    (', ({ char *x[3]; sizeof(x); }));', 'ASSERT(12, ({ char *x[3]; sizeof(x); }));'),
    (', ({ char (*x)[3]; sizeof(x); }));', 'ASSERT(4, ({ char (*x)[3]; sizeof(x); }));'),
])

# pointers are 4 chars
adapt('vla.c', [
    (', ({ char n=10; int (*x)[n][n+2]; sizeof(x); }));', 'ASSERT(4, ({ char n=10; int (*x)[n][n+2]; sizeof(x); }));'),
])

# 36-bit constants; no floating point
adapt('constexpr.c', [
    ('float g40', DEL), ('double g41', DEL), ('g40', DEL), ('g41', DEL),
    ('char x[(int)0xfffffffffff+5]', 'ASSERT(4, ({ char x[(int)0xfffffffff+5]; sizeof(x); }));'),
    ('char x[(unsigned int)0xfffffffffff>>31]', 'ASSERT(31, ({ char x[(unsigned int)0xfffffffff>>31]; sizeof(x); }));'),
    # truncation to 9-bit chars (unsigned) and 18-bit shorts
    ('char x[(char)0xffffff0f]', 'ASSERT(0x10f, ({ char x[(char)0xffffff0f]; sizeof(x); }));'),
    ('char x[(short)0xffff010f]', 'ASSERT(0x10f, ({ char x[(short)0xfff0010f]; sizeof(x); }));'),
    ('char x[(unsigned char)0xffffffff]', 'ASSERT(511, ({ char x[(unsigned char)0xffffffff]; sizeof(x); }));'),
    ('char x[(unsigned short)0xffff800f]', 'ASSERT(0x3800f, ({ char x[(unsigned short)0xffff800f]; sizeof(x); }));'),
    # an int pointer's integer value is a word address
    ('char x[(int*)0+2]', 'ASSERT(2, ({ char x[(int*)0+2]; sizeof(x); }));'),
    ('char x[(int*)16-1]', 'ASSERT(15, ({ char x[(int*)16-1]; sizeof(x); }));'),
    ('char x[(int*)16-(int*)4]', 'ASSERT(12, ({ char x[(int*)16-(int*)4]; sizeof(x); }));'),
    # 1<<62 overflows a 36-bit long
    ('char x[(long)-1/((long)1<<62)+1]', 'ASSERT(1, ({ char x[(long)-1/((long)1<<34)+1]; sizeof(x); }));'),
    ('char x[(unsigned long)-1/((long)1<<62)+1]', 'ASSERT(4, ({ char x[(unsigned long)-1/((long)1<<34)+1]; sizeof(x); }));'),
])
