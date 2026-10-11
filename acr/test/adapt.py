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

def adapt(name, edits=(), drop_float=True, text_edits=()):
    src = open(os.path.join(root, 'test', name)).read()
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
    out[0] += '  // ACR-7000 adaptation of test/%s (see acr/test/adapt.py)' % name
    open(os.path.join(here, name), 'w').write('\n'.join(out))

# --- sizes: every scalar is one word -------------------------------------

adapt('alignof.c', [
    ('_Alignof(short));', 'ASSERT(1, _Alignof(short));'),
    ('_Alignof(int));', 'ASSERT(1, _Alignof(int));'),
    ('_Alignof(long));', 'ASSERT(1, _Alignof(long));'),
    ('_Alignof(long long));', 'ASSERT(1, _Alignof(long long));'),
    ('_Alignof(int[3]));', 'ASSERT(1, _Alignof(int[3]));'),
    ('_Alignof(struct {char a; long b;}[2]));', 'ASSERT(1, _Alignof(struct {char a; long b;}[2]));'),
    ('_Alignas(long) char x, y;', 'ASSERT(1, ({ _Alignas(long) char x, y; &y-&x; }));'),
    # Over-aligned variables are not supported (a warning is issued).
    ('_Alignas(32) char x, y;', DEL),
    ('_Alignas(32) int *x, *y;', DEL),
    ('&g1 % 512', DEL), ('&g2 % 512', DEL),
    ('&g4 % 4', 'ASSERT(0, (long)(char *)&g4 % 1);'),
    ('&g5 % 8', 'ASSERT(0, (long)(char *)&g5 % 1);'),
    ('int x; _Alignof(x);', 'ASSERT(1, ({ int x; _Alignof(x); }));'),
    ('int x; _Alignof x;', 'ASSERT(1, ({ int x; _Alignof x; }));'),
    ('_Alignof(char) << 63 >> 63', 'ASSERT(1, _Alignof(char) << 35 >> 35);'),
    ('_Alignof(x) << 63 >> 63', 'ASSERT(1, ({ char x; _Alignof(x) << 35 >> 35; }));'),
    # x86-64 ABI rule for large arrays
    ('char x[16]; (unsigned long)&x % 16', DEL),
    ('char x[17]; (unsigned long)&x % 16', DEL),
    ('char x[100]; (unsigned long)&x % 16', DEL),
    ('char x[101]; (unsigned long)&x % 16', DEL),
], text_edits=[('int _Alignas(512) g1;\nint _Alignas(512) g2;\n', '')])

# alloca does not round sizes up to 16
adapt('alloca.c', [('ASSERT(16, p2 - p3);', 'ASSERT(3, p2 - p3);')])

adapt('arith.c', [
    # 1073741824 * 100 wraps in 36 bits: 107374182400 - 2^36 is negative
    ('1073741824 * 100 / 100', 'ASSERT(-300647710, 1073741824 * 100 / 100);'),
    ('sizeof(!(char)0)', 'ASSERT(1, sizeof(!(char)0));'),
    ('sizeof(!(long)0)', 'ASSERT(1, sizeof(!(long)0));'),
    ('sizeof(0?1:2)', 'ASSERT(1, sizeof(0?1:2));'),
    ('sizeof(0?(long)1:(long)2)', 'ASSERT(1, sizeof(0?(long)1:(long)2));'),
    # 64-bit pointer constants
    ('(char *)0xffffffffffffffff - (char *)0xfffffffffffffff0', 'ASSERT(15, (char *)0xfffffffff - (char *)0xffffffff0);'),
    ('(char *)0xfffffffffffffff0 - (char *)0xffffffffffffffff', 'ASSERT(-15, (char *)0xffffffff0 - (char *)0xfffffffff);'),
    ('(void *)0xffffffffffffffff > (void *)0', 'ASSERT(1, (void *)0xfffffffff > (void *)0);'),
])

adapt('attribute.c', [
    ('struct { char a; int b; } __attribute__((packed)) x;', 'ASSERT(2, ({ struct { char a; int b; } __attribute__((packed)) x; sizeof(x); }));'),
    ('struct __attribute__((packed)) { char a; int b; } x;', 'ASSERT(2, ({ struct __attribute__((packed)) { char a; int b; } x; sizeof(x); }));'),
    ('typedef struct { char a; int b[2]; } __attribute__((packed)) T;', 'ASSERT(3, ({ typedef struct { char a; int b[2]; } __attribute__((packed)) T; sizeof(T); }));'),
    ('typedef struct __attribute__((packed)) { char a; int b[2]; } T;', 'ASSERT(3, ({ typedef struct __attribute__((packed)) { char a; int b[2]; } T; sizeof(T); }));'),
])

adapt('bitfield.c', [
    ('sizeof(struct {int x:1; })', 'ASSERT(1, sizeof(struct {int x:1; }));'),
    ('sizeof(struct {long x:1; })', 'ASSERT(1, sizeof(struct {long x:1; }));'),
    # short a, char b and the three bit-fields each take a word
    ('ASSERT(4, sizeof(struct bit1));', 'ASSERT(3, sizeof(struct bit1));'),
    ('sizeof(struct {int a:3; int c:1; int c:5;})', 'ASSERT(1, sizeof(struct {int a:3; int c:1; int c:5;}));'),
    ('sizeof(struct {int a:3; int:0; int c:5;})', 'ASSERT(2, sizeof(struct {int a:3; int:0; int c:5;}));'),
    ('sizeof(struct {int a:3; int:0;})', 'ASSERT(1, sizeof(struct {int a:3; int:0;}));'),
], text_edits=[('  printf("OK\\n");', '''  // 36-bit storage units
  ASSERT(1, sizeof(struct {int a:12; int b:12; int c:12;}));
  ASSERT(2, sizeof(struct {int a:12; int b:12; int c:13;}));
  ASSERT(-1, ({ struct {int a:12; int b:12; int c:12;} x={0,0,-1}; x.c; }));
  ASSERT(4095, ({ struct {unsigned a:12; unsigned b:12; unsigned c:12;} x={0,0,-1}; x.c; }));
  ASSERT(34359738367, ({ struct {int a:36;} x={34359738367}; x.a; }));
  ASSERT(-1, ({ struct {int a:35; int b:1;} x={0,-1}; x.b; }));

  printf("OK\\n");''')])

adapt('cast.c', [
    # 8590066177 = 2^33 + 131585 fits in a word, so nothing is truncated
    ('(int)8590066177', 'ASSERT(8590066177, (int)8590066177);'),
    ('(short)8590066177', 'ASSERT(8590066177, (short)8590066177);'),
    ('(char)8590066177', 'ASSERT(8590066177, (char)8590066177);'),
    ('*(char *)&x=1', 'ASSERT(1, ({ int x=512; *(char *)&x=1; x; }));'),
    ('(char)255', 'ASSERT(255, (char)255);'),
    ('(signed char)255', 'ASSERT(255, (signed char)255);'),
    ('(short)65535', 'ASSERT(65535, (short)65535);'),
    ('(int)0xffffffff', 'ASSERT(4294967295, (int)0xffffffff);'),
    ('((unsigned)-1)>>1', 'ASSERT(34359738367, ((unsigned)-1)>>1);'),
    ('((unsigned)-100)/2', 'ASSERT(34359738318, ((unsigned)-100)/2);'),
    ('((unsigned long)-100)/2', 'ASSERT(34359738318, ((unsigned long)-100)/2);'),
    # unsigned has lower rank but is as wide as long: unsigned long
    ('((long)-1)/(unsigned)100', 'ASSERT(687194767, ((long)-1)/(unsigned)100);'),
    ('((unsigned)-100)%7', 'ASSERT(6, ((unsigned)-100)%7);'),
    ('((unsigned long)-100)%9', 'ASSERT(0, ((unsigned long)-100)%9);'),
    ('typedef short T; T x = 65535;', 'ASSERT(65535, ({ typedef short T; T x = 65535; (int)x; }));'),
    # same tests at the 36-bit boundary
], text_edits=[('  printf("OK\\n");', '''  ASSERT(-1, (int)68719476735U);
  ASSERT(-1, (char)68719476735U);
  ASSERT(68719476735, (unsigned char)-1);
  ASSERT(1, (unsigned char)-1 > 0);
  ASSERT(34359738367, ((unsigned)-1)>>1);
  ASSERT(-34359738368, (int)34359738368U);

  printf("OK\\n");''')])

adapt('constexpr.c', [
    ('char x[(int)0xfffffffffff+5]', 'ASSERT(4, ({ char x[(int)0xfffffffff+5]; sizeof(x); }));'),
    # char is not narrower than int, so there is nothing to truncate
    ('int x[1+1]; sizeof(x);', 'ASSERT(2, ({ int x[1+1]; sizeof(x); }));'),
    ('char x[(char)0xffffff0f]', DEL),
    ('char x[(short)0xffff010f]', DEL),
    ('char x[(unsigned char)0xffffffff]', DEL),
    ('char x[(unsigned short)0xffff800f]', DEL),
    ('char x[(int*)0+2]', 'ASSERT(2, ({ char x[(int*)0+2]; sizeof(x); }));'),
    ('char x[(int*)16-1]', 'ASSERT(15, ({ char x[(int*)16-1]; sizeof(x); }));'),
    ('char x[(int*)16-(int*)4]', 'ASSERT(12, ({ char x[(int*)16-(int*)4]; sizeof(x); }));'),
    ('char x[(long)-1/((long)1<<62)+1]', 'ASSERT(1, ({ char x[(long)-1/((long)1<<34)+1]; sizeof(x); }));'),
    ('char x[(unsigned long)-1/((long)1<<62)+1]', 'ASSERT(4, ({ char x[(unsigned long)-1/((long)1<<34)+1]; sizeof(x); }));'),
    ('char x[(unsigned int)0xfffffffffff>>31]', 'ASSERT(31, ({ char x[(unsigned int)0xfffffffff>>31]; sizeof(x); }));'),
    ('float g40', DEL), ('double g41', DEL),
    ('g40', DEL), ('g41', DEL),
])

adapt('control.c', [
    # 0xffffffff is not -1 in a 36-bit int; use an all-ones 36-bit value
    ('switch(-1) { case 0xffffffff: i=3; break; }', 'ASSERT(3, ({ int i=0; switch(-1) { case 0xfffffffff: i=3; break; } i; }));'),
])

adapt('decl.c', [
    ('short int x; sizeof(x);', 'ASSERT(1, ({ short int x; sizeof(x); }));'),
    ('int short x; sizeof(x);', 'ASSERT(1, ({ int short x; sizeof(x); }));'),
    ('({ int x; sizeof(x); })', 'ASSERT(1, ({ int x; sizeof(x); }));'),
    ('long int x; sizeof(x);', 'ASSERT(1, ({ long int x; sizeof(x); }));'),
    ('int long x; sizeof(x);', 'ASSERT(1, ({ int long x; sizeof(x); }));'),
    ('long long x; sizeof(x);', 'ASSERT(1, ({ long long x; sizeof(x); }));'),
    # (char)256 is 256: char has 36 bits
    ('(_Bool)(char)256', 'ASSERT(1, (_Bool)(char)256);'),
])

adapt('enum.c', [
    ('enum { zero, one, two } x; sizeof(x);', 'ASSERT(1, ({ enum { zero, one, two } x; sizeof(x); }));'),
    ('enum t y; sizeof(y);', 'ASSERT(1, ({ enum t { zero, one, two }; enum t y; sizeof(y); }));'),
])

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
    # char and short are as wide as int: nothing is truncated
    ('ASSERT(5, int_to_char(261));', 'ASSERT(261, int_to_char(261));'),
    ('ASSERT(3, char_fn());', 'ASSERT(515, char_fn());'),
    ('ASSERT(5, short_fn());', 'ASSERT(131077, short_fn());'),
    ('ASSERT(251, uchar_fn());', 'ASSERT(2043, uchar_fn());'),
    ('ASSERT(65528, ushort_fn());', 'ASSERT(2097144, ushort_fn());'),
    ('ASSERT(-5, schar_fn());', 'ASSERT(2043, schar_fn());'),
    ('ASSERT(-8, sshort_fn());', 'ASSERT(2097144, sshort_fn());'),
    # true_fn/false_fn are defined as returning int but declared here as
    # returning _Bool; x86-64 narrows the return register at the call,
    # the ACR-7000 ABI passes the word through.
    ('ASSERT(1, true_fn());', 'ASSERT(513, true_fn());'),
    ('ASSERT(0, false_fn());', 'ASSERT(512, false_fn());'),
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

adapt('initializer.c', [
    ('int x[]={1,2,3,4}; sizeof(x);', 'ASSERT(4, ({ int x[]={1,2,3,4}; sizeof(x); }));'),
    # No type punning between int and char arrays: each char is a word,
    # so b[0] aliases all of a.
    ('union { int a; char b[4]; } x={0x01020304}; x.b[0];', 'ASSERT(0x01020304, ({ union { int a; char b[4]; } x={0x01020304}; x.b[0]; }));'),
    ('union { int a; char b[4]; } x={0x01020304}; x.b[1];', 'ASSERT(0, ({ union { int a; char b[4]; } x={0x01020304}; x.b[1]; }));'),
    ('union { struct { char a,b,c,d; } e; int f; } x={{4,3,2,1}}; x.f;', 'ASSERT(4, ({ union { struct { char a,b,c,d; } e; int f; } x={{4,3,2,1}}; x.f; }));'),
    ('ASSERT(4, g13[0].b[0]);', 'ASSERT(0x01020304, g13[0].b[0]);'),
    ('ASSERT(3, g13[0].b[1]);', 'ASSERT(0, g13[0].b[1]);'),
    ('ASSERT(8, g13[1].b[0]);', 'ASSERT(0x05060708, g13[1].b[0]);'),
    ('ASSERT(7, g13[1].b[1]);', 'ASSERT(0, g13[1].b[1]);'),
    ('union { unsigned short a; char b[2]; } x={.b[1]=0xff}; x.a;', 'ASSERT(0, ({ union { unsigned short a; char b[2]; } x={.b[1]=0xff}; x.a; }));'),
    ('ASSERT(0x00120000, g50.a);', 'ASSERT(0, g50.a);'),
])

adapt('literal.c', [
    ('sizeof(0));', 'ASSERT(1, sizeof(0));'),
    ('sizeof(0L));', 'ASSERT(1, sizeof(0L));'),
    ('sizeof(0LU));', 'ASSERT(1, sizeof(0LU));'),
    ('sizeof(0UL));', 'ASSERT(1, sizeof(0UL));'),
    ('sizeof(0LL));', 'ASSERT(1, sizeof(0LL));'),
    ('sizeof(0LLU));', 'ASSERT(1, sizeof(0LLU));'),
    ('sizeof(0Ull));', 'ASSERT(1, sizeof(0Ull));'),
    ('sizeof(0l));', 'ASSERT(1, sizeof(0l));'),
    ('sizeof(0ll));', 'ASSERT(1, sizeof(0ll));'),
    ('sizeof(0x0L));', 'ASSERT(1, sizeof(0x0L));'),
    ('sizeof(0b0L));', 'ASSERT(1, sizeof(0b0L));'),
    ('ASSERT(4, sizeof(2147483647));', 'ASSERT(1, sizeof(2147483647));'),
    ('ASSERT(8, sizeof(2147483648));', 'ASSERT(1, sizeof(2147483648));'),
    ('ASSERT(-1, 0xffffffffffffffff);', 'ASSERT(-1, 0xfffffffff);'),
    ('ASSERT(8, sizeof(0xffffffffffffffff));', 'ASSERT(1, sizeof(0xfffffffff));'),
    ('ASSERT(4, sizeof(4294967295U));', 'ASSERT(1, sizeof(4294967295U));'),
    ('ASSERT(8, sizeof(4294967296U));', 'ASSERT(1, sizeof(4294967296U));'),
    ('-1U>>30', 'ASSERT(63, -1U>>30);'),
    ('-1Ul>>62', 'ASSERT(3, -1Ul>>34);'),
    ('-1ull>>62', 'ASSERT(3, -1ull>>34);'),
    ('0xffffffffffffffffl>>63', 'ASSERT(1, 0xfffffffffl>>35);'),
    ('0xffffffffffffffffll>>63', 'ASSERT(1, 0xfffffffffll>>35);'),
    ('ASSERT(-1, 18446744073709551615);', 'ASSERT(-1, 68719476735);'),
    ('ASSERT(8, sizeof(18446744073709551615));', 'ASSERT(1, sizeof(68719476735));'),
    # an unsuffixed decimal constant too large for long becomes unsigned
    ('18446744073709551615>>63', 'ASSERT(1, 68719476735>>35);'),
    ('0xffffffffffffffff>>63', 'ASSERT(1, 0xfffffffff>>35);'),
    ('ASSERT(-1, 01777777777777777777777);', 'ASSERT(-1, 0777777777777);'),
    ('sizeof(01777777777777777777777)', 'ASSERT(1, sizeof(0777777777777));'),
    ('01777777777777777777777>>63', 'ASSERT(1, 0777777777777>>35);'),
    ('ASSERT(-1, 0b1111111111111111111111111111111111111111111111111111111111111111);',
     'ASSERT(-1, 0b111111111111111111111111111111111111);'),
    ('sizeof(0b1111111111111111111111111111111111111111111111111111111111111111)',
     'ASSERT(1, sizeof(0b111111111111111111111111111111111111));'),
    ('0b1111111111111111111111111111111111111111111111111111111111111111>>63',
     'ASSERT(1, 0b111111111111111111111111111111111111>>35);'),
    ('sizeof(0x1ffffffff)', 'ASSERT(1, sizeof(0x1ffffffff));'),
    ('sizeof(0xffffffff)', 'ASSERT(1, sizeof(0xffffffff));'),
    ('0xffffffff>>31', 'ASSERT(1, 0xffffffff>>31);'),
    ('sizeof(040000000000)', 'ASSERT(1, sizeof(040000000000));'),
    ('sizeof(037777777777)', 'ASSERT(1, sizeof(037777777777));'),
    ('037777777777>>31', 'ASSERT(1, 037777777777>>31);'),
    ('sizeof(0b111111111111111111111111111111111)', 'ASSERT(1, sizeof(0b111111111111111111111111111111111));'),
    ('sizeof(0b11111111111111111111111111111111)', 'ASSERT(1, sizeof(0b11111111111111111111111111111111));'),
    ('0b11111111111111111111111111111111>>31', 'ASSERT(1, 0b11111111111111111111111111111111>>31);'),
    ('ASSERT(-1, 1 << 31 >> 31);', 'ASSERT(-1, 1 << 35 >> 35);'),
    ('ASSERT(-1, 01 << 31 >> 31);', 'ASSERT(-1, 01 << 35 >> 35);'),
    ('ASSERT(-1, 0x1 << 31 >> 31);', 'ASSERT(-1, 0x1 << 35 >> 35);'),
    ('ASSERT(-1, 0b1 << 31 >> 31);', 'ASSERT(-1, 0b1 << 35 >> 35);'),
    ("sizeof(L'\\0')", "ASSERT(1, sizeof(L'\\0'));"),
    ('sizeof(8f)', 'ASSERT(1, sizeof(8f));'),
    # char is 36 bits wide, so '\x80' is positive
    ("ASSERT(-128, '\\x80');", "ASSERT(128, '\\x80');"),
])

adapt('macro.c', [
    ('strcmp(main_filename1, "test/macro.c")', 'ASSERT(0, strcmp(main_filename1, "acr/test/macro.c"));'),
    ('strcmp(__BASE_FILE__, "test/macro.c")', 'ASSERT(0, strcmp(__BASE_FILE__, "acr/test/macro.c"));'),
])

adapt('offsetof.c', [
    ('double d;', 'long d;'),
    ('offsetof(T, b)', 'ASSERT(1, offsetof(T, b));'),
    ('offsetof(T, c)', 'ASSERT(2, offsetof(T, c));'),
    ('offsetof(T, d)', 'ASSERT(3, offsetof(T, d));'),
])

adapt('sizeof.c', [
    ('sizeof(short));', 'ASSERT(1, sizeof(short));'),
    ('sizeof(short int));', 'ASSERT(1, sizeof(short int));'),
    ('sizeof(int short));', 'ASSERT(1, sizeof(int short));'),
    ('sizeof(int));', 'ASSERT(1, sizeof(int));'),
    ('sizeof(long));', 'ASSERT(1, sizeof(long));'),
    ('sizeof(long int));', 'ASSERT(1, sizeof(long int));'),
    ('sizeof(char *));', 'ASSERT(1, sizeof(char *));'),
    ('sizeof(int *));', 'ASSERT(1, sizeof(int *));'),
    ('sizeof(long *));', 'ASSERT(1, sizeof(long *));'),
    ('sizeof(int **));', 'ASSERT(1, sizeof(int **));'),
    ('sizeof(int(*)[4]));', 'ASSERT(1, sizeof(int(*)[4]));'),
    ('sizeof(int*[4]));', 'ASSERT(4, sizeof(int*[4]));'),
    ('sizeof(int[4]));', 'ASSERT(4, sizeof(int[4]));'),
    ('sizeof(int[3][4]));', 'ASSERT(12, sizeof(int[3][4]));'),
    ('sizeof(struct {int a; int b;}));', 'ASSERT(2, sizeof(struct {int a; int b;}));'),
    ('sizeof(-10 + (long)5)', 'ASSERT(1, sizeof(-10 + (long)5));'),
    ('sizeof(-10 - (long)5)', 'ASSERT(1, sizeof(-10 - (long)5));'),
    ('sizeof(-10 * (long)5)', 'ASSERT(1, sizeof(-10 * (long)5));'),
    ('sizeof(-10 / (long)5)', 'ASSERT(1, sizeof(-10 / (long)5));'),
    ('sizeof((long)-10 + 5)', 'ASSERT(1, sizeof((long)-10 + 5));'),
    ('sizeof((long)-10 - 5)', 'ASSERT(1, sizeof((long)-10 - 5));'),
    ('sizeof((long)-10 * 5)', 'ASSERT(1, sizeof((long)-10 * 5));'),
    ('sizeof((long)-10 / 5)', 'ASSERT(1, sizeof((long)-10 / 5));'),
    ('sizeof(int(*)[10]));', 'ASSERT(1, sizeof(int(*)[10]));'),
    ('sizeof(int(*)[][10]));', 'ASSERT(1, sizeof(int(*)[][10]));'),
    ('sizeof(struct { int x, y[]; })', 'ASSERT(1, sizeof(struct { int x, y[]; }));'),
    ('sizeof(signed short));', 'ASSERT(1, sizeof(signed short));'),
    ('sizeof(int short signed));', 'ASSERT(1, sizeof(int short signed));'),
    ('sizeof(unsigned short));', 'ASSERT(1, sizeof(unsigned short));'),
    ('sizeof(int short unsigned));', 'ASSERT(1, sizeof(int short unsigned));'),
    ('sizeof(signed int));', 'ASSERT(1, sizeof(signed int));'),
    ('sizeof(signed));', 'ASSERT(1, sizeof(signed));'),
    ('sizeof(signed signed));', 'ASSERT(1, sizeof(signed signed));'),
    ('sizeof(unsigned int));', 'ASSERT(1, sizeof(unsigned int));'),
    ('sizeof(unsigned));', 'ASSERT(1, sizeof(unsigned));'),
    ('sizeof(unsigned unsigned));', 'ASSERT(1, sizeof(unsigned unsigned));'),
    ('sizeof(signed long));', 'ASSERT(1, sizeof(signed long));'),
    ('sizeof(signed long int));', 'ASSERT(1, sizeof(signed long int));'),
    ('sizeof(unsigned long));', 'ASSERT(1, sizeof(unsigned long));'),
    ('sizeof(unsigned long int));', 'ASSERT(1, sizeof(unsigned long int));'),
    ('sizeof(long long));', 'ASSERT(1, sizeof(long long));'),
    ('sizeof(signed long long));', 'ASSERT(1, sizeof(signed long long));'),
    ('sizeof(signed long long int));', 'ASSERT(1, sizeof(signed long long int));'),
    ('sizeof(unsigned long long));', 'ASSERT(1, sizeof(unsigned long long));'),
    ('sizeof(unsigned long long int));', 'ASSERT(1, sizeof(unsigned long long int));'),
    ('sizeof((short)1)', 'ASSERT(1, sizeof((short)1));'),
    ('sizeof((int)1)', 'ASSERT(1, sizeof((int)1));'),
    ('sizeof((long)1)', 'ASSERT(1, sizeof((long)1));'),
    ('sizeof((char)1 + (char)1)', 'ASSERT(1, sizeof((char)1 + (char)1));'),
    ('sizeof((short)1 + (short)1)', 'ASSERT(1, sizeof((short)1 + (short)1));'),
    ('sizeof(1?2:3)', 'ASSERT(1, sizeof(1?2:3));'),
    ('sizeof(1?(short)2:(char)3)', 'ASSERT(1, sizeof(1?(short)2:(char)3));'),
    ('sizeof(1?(long)2:(char)3)', 'ASSERT(1, sizeof(1?(long)2:(char)3));'),
    ('sizeof(char) << 63 >> 63', 'ASSERT(1, sizeof(char) << 35 >> 35);'),
    ('sizeof(1f+2)', 'ASSERT(1, sizeof(1f+2));'),
    ('sizeof(1f-2)', 'ASSERT(1, sizeof(1f-2));'),
    ('sizeof(1f*2)', 'ASSERT(1, sizeof(1f*2));'),
    ('sizeof(1f/2)', 'ASSERT(1, sizeof(1f/2));'),
])

adapt('string.c', [
    ('sizeof(L"abc" "")', 'ASSERT(4, sizeof(L"abc" ""));'),
    ('sizeof(L"abc" "def")', 'ASSERT(7, sizeof(L"abc" "def"));'),
    ('sizeof(L"abc" L"def")', 'ASSERT(7, sizeof(L"abc" L"def"));'),
    ('sizeof(u"abc" "def")', 'ASSERT(7, sizeof(u"abc" "def"));'),
    ('sizeof(u"abc" u"def")', 'ASSERT(7, sizeof(u"abc" u"def"));'),
])

adapt('struct.c', [
    ('struct {int a;} x; sizeof(x);', 'ASSERT(1, ({ struct {int a;} x; sizeof(x); }));'),
    ('struct {int a; int b;} x; sizeof(x);', 'ASSERT(2, ({ struct {int a; int b;} x; sizeof(x); }));'),
    ('struct {int a, b;} x; sizeof(x);', 'ASSERT(2, ({ struct {int a, b;} x; sizeof(x); }));'),
    ('struct {int a[3];} x; sizeof(x);', 'ASSERT(3, ({ struct {int a[3];} x; sizeof(x); }));'),
    ('struct {int a;} x[4]; sizeof(x);', 'ASSERT(4, ({ struct {int a;} x[4]; sizeof(x); }));'),
    ('struct {int a[3];} x[2]; sizeof(x);', 'ASSERT(6, ({ struct {int a[3];} x[2]; sizeof(x); }));'),
    ('struct {char a; int b;} x; sizeof(x);', 'ASSERT(2, ({ struct {char a; int b;} x; sizeof(x); }));'),
    ('struct {int a; char b;} x; sizeof(x);', 'ASSERT(2, ({ struct {int a; char b;} x; sizeof(x); }));'),
    ('struct t {int a; int b;} x; struct t y; sizeof(y);', 'ASSERT(2, ({ struct t {int a; int b;} x; struct t y; sizeof(y); }));'),
    ('struct t {int a; int b;}; struct t y; sizeof(y);', 'ASSERT(2, ({ struct t {int a; int b;}; struct t y; sizeof(y); }));'),
    ('struct {char a; long b;} x; sizeof(x);', 'ASSERT(2, ({ struct {char a; long b;} x; sizeof(x); }));'),
    ('struct {char a; short b;} x; sizeof(x);', 'ASSERT(2, ({ struct {char a; short b;} x; sizeof(x); }));'),
    ('struct foo *bar; sizeof(bar);', 'ASSERT(1, ({ struct foo *bar; sizeof(bar); }));'),
    ('struct T *foo; struct T {int x;}; sizeof(struct T);', 'ASSERT(1, ({ struct T *foo; struct T {int x;}; sizeof(struct T); }));'),
    ('typedef struct T T; struct T { int x; }; sizeof(T);', 'ASSERT(1, ({ typedef struct T T; struct T { int x; }; sizeof(T); }));'),
])

adapt('typedef.c', [
    ('typedef t; t x; sizeof(x);', 'ASSERT(1, ({ typedef t; t x; sizeof(x); }));'),
    ('MyInt2 x; sizeof(x);', 'ASSERT(4, ({ MyInt2 x; sizeof(x); }));'),
])

adapt('typeof.c', [
    ('int x; typeof(x) y; sizeof(y);', 'ASSERT(1, ({ int x; typeof(x) y; sizeof(y); }));'),
    ('int x; typeof(&x) y; sizeof(y);', 'ASSERT(1, ({ int x; typeof(&x) y; sizeof(y); }));'),
    ('sizeof(typeof(struct { int a,b,c; }))', 'ASSERT(3, sizeof(typeof(struct { int a,b,c; })));'),
])

adapt('union.c', [
    # The members overlap, but every char is a whole word: no punning.
    ('union { int a; char b[6]; } x; sizeof(x);', 'ASSERT(6, ({ union { int a; char b[6]; } x; sizeof(x); }));'),
    ('x.a = 515; x.b[0];', 'ASSERT(515, ({ union { int a; char b[4]; } x; x.a = 515; x.b[0]; }));'),
    ('x.a = 515; x.b[1];', 'ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[1]; }));'),
    ('x.e=0xdeadbeef; x.a;', 'ASSERT(0xdeadbeef, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.a; }));'),
    ('x.e=0xdeadbeef; x.b;', 'ASSERT(0, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.b; }));'),
    ('x.e=0xdeadbeef; x.c;', 'ASSERT(0, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.c; }));'),
    ('x.e=0xdeadbeef; x.d;', 'ASSERT(0, ({ union { struct { unsigned char a,b,c,d; }; long e; } x; x.e=0xdeadbeef; x.d; }));'),
])

USUALCONV_EXTRA = r'''  ASSERT(0, 34359738367 + 34359738367 + 2);
  ASSERT(1, (unsigned char)1 - 2 > 0);
  ASSERT(0, (int)(unsigned char)1 - 2 > 0);

  printf("OK\n");'''

adapt('usualconv.c', [
    # no overflow in 36 bits
    ('2147483647 + 2147483647 + 2', 'ASSERT(4294967296, 2147483647 + 2147483647 + 2);'),
], text_edits=[('  printf("OK\\n");', USUALCONV_EXTRA)])

adapt('variable.c', [
    ('({ int x; sizeof(x); })', 'ASSERT(1, ({ int x; sizeof(x); }));'),
    ('({ int x; sizeof x; })', 'ASSERT(1, ({ int x; sizeof x; }));'),
    ('({ int *x; sizeof(x); })', 'ASSERT(1, ({ int *x; sizeof(x); }));'),
    ('({ int x[4]; sizeof(x); })', 'ASSERT(4, ({ int x[4]; sizeof(x); }));'),
    ('({ int x[3][4]; sizeof(x); })', 'ASSERT(12, ({ int x[3][4]; sizeof(x); }));'),
    ('({ int x[3][4]; sizeof(*x); })', 'ASSERT(4, ({ int x[3][4]; sizeof(*x); }));'),
    ('({ int x[3][4]; sizeof(**x); })', 'ASSERT(1, ({ int x[3][4]; sizeof(**x); }));'),
    ('({ int x[3][4]; sizeof(**x) + 1; })', 'ASSERT(2, ({ int x[3][4]; sizeof(**x) + 1; }));'),
    ('({ int x[3][4]; sizeof **x + 1; })', 'ASSERT(2, ({ int x[3][4]; sizeof **x + 1; }));'),
    ('({ int x[3][4]; sizeof(**x + 1); })', 'ASSERT(1, ({ int x[3][4]; sizeof(**x + 1); }));'),
    ('({ int x=1; sizeof(x=2); })', 'ASSERT(1, ({ int x=1; sizeof(x=2); }));'),
    ('ASSERT(4, sizeof(g1));', 'ASSERT(1, sizeof(g1));'),
    ('ASSERT(16, sizeof(g2));', 'ASSERT(4, sizeof(g2));'),
    # depends on the x86-64 frame layout
    ('char *a=&y; char *b=&z; b-a;', DEL),
    ('({ long x; sizeof(x); })', 'ASSERT(1, ({ long x; sizeof(x); }));'),
    ('({ short x; sizeof(x); })', 'ASSERT(1, ({ short x; sizeof(x); }));'),
    ('({ char *x[3]; sizeof(x); })', 'ASSERT(3, ({ char *x[3]; sizeof(x); }));'),
    ('({ char (*x)[3]; sizeof(x); })', 'ASSERT(1, ({ char (*x)[3]; sizeof(x); }));'),
])

adapt('vla.c', [
    ('int n=5; int x[n]; sizeof(x);', 'ASSERT(5, ({ int n=5; int x[n]; sizeof(x); }));'),
    ('int x[m+1][n*2]; sizeof(x);', 'ASSERT(96, ({ int m=5, n=8; int x[m+1][n*2]; sizeof(x); }));'),
    ('int (*x)[n][n+2]; sizeof(x);', 'ASSERT(1, ({ char n=10; int (*x)[n][n+2]; sizeof(x); }));'),
    ('int (*x)[n][n+2]; sizeof(*x);', 'ASSERT(120, ({ char n=10; int (*x)[n][n+2]; sizeof(*x); }));'),
    ('int (*x)[n][n+2]; sizeof(**x);', 'ASSERT(12, ({ char n=10; int (*x)[n][n+2]; sizeof(**x); }));'),
    ('int (*x)[n][n+2]; sizeof(***x);', 'ASSERT(1, ({ char n=10; int (*x)[n][n+2]; sizeof(***x); }));'),
    ('char n=3; int x[5][n]; sizeof(x);', 'ASSERT(15, ({ char n=3; int x[5][n]; sizeof(x); }));'),
    ('char n=3; int x[5][n]; sizeof(*x);', 'ASSERT(3, ({ char n=3; int x[5][n]; sizeof(*x); }));'),
    ('char n=3; int x[n][5]; sizeof(x);', 'ASSERT(15, ({ char n=3; int x[n][5]; sizeof(x); }));'),
    ('char n=3; int x[n][5]; sizeof(*x);', 'ASSERT(5, ({ char n=3; int x[n][5]; sizeof(*x); }));'),
    ('i<sizeof(x)/4;', 'ASSERT(82, ({ int n=10; int x[n+1][n+6]; int *p=x; for (int i = 0; i<sizeof(x)/sizeof(int); i++) p[i]=i; x[5][2]; }));'),
])

adapt('unicode.c', [
    ("sizeof(L'\\0')", "ASSERT(1, sizeof(L'\\0'));"),
    # wchar_t is a 36-bit int, so 0xffffffff is positive
    ("L'\\xffffffff'>>31", "ASSERT(1, L'\\xffffffff'>>31);"),
    ("sizeof(u'\\0')", "ASSERT(1, sizeof(u'\\0'));"),
    ("sizeof(U'\\0')", "ASSERT(1, sizeof(U'\\0'));"),
    ('sizeof(u"")', 'ASSERT(1, sizeof(u""));'),
    ('sizeof(u"\\xffzzz")', 'ASSERT(5, sizeof(u"\\xffzzz"));'),
    ('sizeof(U"")', 'ASSERT(1, sizeof(U""));'),
    ('sizeof(U"\\xffzzz")', 'ASSERT(5, sizeof(U"\\xffzzz"));'),
    ('sizeof(L"")', 'ASSERT(1, sizeof(L""));'),
    ('sizeof(L"\\xffzzz")', 'ASSERT(5, sizeof(L"\\xffzzz"));'),
    ('L"\\xffffffff"[0] >> 31', 'ASSERT(1, L"\\xffffffff"[0] >> 31);'),
    ('char16_t x[] = u"αβ"; sizeof(x);', 'ASSERT(3, ({ char16_t x[] = u"αβ"; sizeof(x); }));'),
    ('char32_t x[] = U"🤔x"; sizeof(x);', 'ASSERT(3, ({ char32_t x[] = U"🤔x"; sizeof(x); }));'),
    ('wchar_t x[] = L"🤔x"; sizeof(x);', 'ASSERT(3, ({ wchar_t x[] = L"🤔x"; sizeof(x); }));'),
    # byte-level layout of wide strings: compare elements instead
    ('memcmp(u"", "\\0\\0", 2)', 'ASSERT(0, u""[0]);'),
    ('memcmp(u"abc", ', 'ASSERT(0, memcmp(u"abc", (char16_t[]){97, 98, 99, 0}, 4));'),
    ('memcmp(u"日本語", ', 'ASSERT(0, memcmp(u"日本語", (char16_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 4));'),
    ('memcmp(u"🍣", ', 'ASSERT(0, memcmp(u"🍣", (char16_t[]){0xd83c, 0xdf63, 0}, 3));'),
    ('memcmp(U"", "\\0\\0\\0\\0", 4)', 'ASSERT(0, U""[0]);'),
    ('memcmp(U"abc", ', 'ASSERT(0, memcmp(U"abc", (char32_t[]){97, 98, 99, 0}, 4));'),
    ('memcmp(U"日本語", ', 'ASSERT(0, memcmp(U"日本語", (char32_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 4));'),
    ('memcmp(U"🍣", ', 'ASSERT(0, memcmp(U"🍣", (char32_t[]){0x1f363, 0}, 2));'),
    ('memcmp(L"", "\\0\\0\\0\\0", 4)', 'ASSERT(0, L""[0]);'),
    ('memcmp(L"abc", ', 'ASSERT(0, memcmp(L"abc", (wchar_t[]){97, 98, 99, 0}, 4));'),
    ('memcmp(L"日本語", ', 'ASSERT(0, memcmp(L"日本語", (wchar_t[]){0x65e5, 0x672c, 0x8a9e, 0}, 4));'),
    ('memcmp(L"🍣", ', 'ASSERT(0, memcmp(L"🍣", (wchar_t[]){0x1f363, 0}, 2));'),
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
