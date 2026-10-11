#!/bin/bash
# ACR-7000 version of test/driver.sh: programs are run on the emulator
# (acrrun) and host-only features are checked to be rejected.
chibicc=$1
run="$(dirname $chibicc)/acrrun -q"

tmp=`mktemp -d /tmp/chibicc-test-XXXXXX`
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > $tmp/empty.c

check() {
    if [ $? -eq 0 ]; then
        echo "testing $1 ... passed"
    else
        echo "testing $1 ... failed"
        exit 1
    fi
}

# -o
rm -f $tmp/out
./chibicc -c -o $tmp/out $tmp/empty.c
[ -f $tmp/out ]
check -o

# --help
$chibicc --help 2>&1 | grep -q chibicc
check --help

# -S
echo 'int main() {}' | $chibicc -S -o- -xc - | grep -q 'main:'
check -S

# Default output file
rm -f $tmp/out.o $tmp/out.s
echo 'int main() {}' > $tmp/out.c
(cd $tmp; $OLDPWD/$chibicc -c out.c)
[ -f $tmp/out.o ]
check 'default output file'

(cd $tmp; $OLDPWD/$chibicc -c -S out.c)
[ -f $tmp/out.s ]
check 'default output file'

# Multiple input files
rm -f $tmp/foo.o $tmp/bar.o
echo 'int x;' > $tmp/foo.c
echo 'int y;' > $tmp/bar.c
(cd $tmp; $OLDPWD/$chibicc -c $tmp/foo.c $tmp/bar.c)
[ -f $tmp/foo.o ] && [ -f $tmp/bar.o ]
check 'multiple input files'

rm -f $tmp/foo.s $tmp/bar.s
echo 'int x;' > $tmp/foo.c
echo 'int y;' > $tmp/bar.c
(cd $tmp; $OLDPWD/$chibicc -c -S $tmp/foo.c $tmp/bar.c)
[ -f $tmp/foo.s ] && [ -f $tmp/bar.s ]
check 'multiple input files'

# Run linker
rm -f $tmp/foo
echo 'int main() { return 0; }' | $chibicc -o $tmp/foo -xc -xc -
$run $tmp/foo
check linker

rm -f $tmp/foo
echo 'int bar(); int main() { return bar(); }' > $tmp/foo.c
echo 'int bar() { return 42; }' > $tmp/bar.c
$chibicc -o $tmp/foo $tmp/foo.c $tmp/bar.c
$run $tmp/foo
[ "$?" = 42 ]
check linker

# a.out
rm -f $tmp/a.out
echo 'int main() {}' > $tmp/foo.c
(cd $tmp; $OLDPWD/$chibicc foo.c)
[ -f $tmp/a.out ]
check a.out

# -E
echo foo > $tmp/out
echo "#include \"$tmp/out\"" | $chibicc -E -xc - | grep -q foo
check -E

echo foo > $tmp/out1
echo "#include \"$tmp/out1\"" | $chibicc -E -o $tmp/out2 -xc -
cat $tmp/out2 | grep -q foo
check '-E and -o'

# -I
mkdir $tmp/dir
echo foo > $tmp/dir/i-option-test
echo "#include \"i-option-test\"" | $chibicc -I$tmp/dir -E -xc - | grep -q foo
check -I

# -D
echo foo | $chibicc -Dfoo -E -xc - | grep -q 1
check -D

# -D
echo foo | $chibicc -Dfoo=bar -E -xc - | grep -q bar
check -D

# -U
echo foo | $chibicc -Dfoo=bar -Ufoo -E -xc - | grep -q foo
check -U

# ignored options
$chibicc -c -O -Wall -g -std=c11 -ffreestanding -fno-builtin \
         -fno-omit-frame-pointer -fno-stack-protector -fno-strict-aliasing \
         -m64 -mno-red-zone -w -o /dev/null $tmp/empty.c
check 'ignored options'

# BOM marker
printf '\xef\xbb\xbfxyz\n' | $chibicc -E -o- -xc - | grep -q '^xyz'
check 'BOM marker'

# Inline functions
echo 'inline void foo() {}' > $tmp/inline1.c
echo 'inline void foo() {}' > $tmp/inline2.c
echo 'int main() { return 0; }' > $tmp/inline3.c
$chibicc -o /dev/null $tmp/inline1.c $tmp/inline2.c $tmp/inline3.c
check inline

echo 'extern inline void foo() {}' > $tmp/inline1.c
echo 'int foo(); int main() { foo(); }' > $tmp/inline2.c
$chibicc -o /dev/null $tmp/inline1.c $tmp/inline2.c
check inline

echo 'static inline void f1() {}' | $chibicc -o- -S -xc - | grep -v -q '^; f1$'
check inline

echo 'static inline void f1() {} void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q '^; f1$'
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q '^; f1$'
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -v -q '^; f2$'
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q '^; f1$'
check inline

echo 'static inline void f1() {} static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q '^; f2$'
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() {}' | $chibicc -o- -S -xc - | grep -v -q '^; f1$'
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() {}' | $chibicc -o- -S -xc - | grep -v -q '^; f2$'
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q '^; f1$'
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f1(); }' | $chibicc -o- -S -xc - | grep -q '^; f2$'
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q '^; f1$'
check inline

echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void foo() { f2(); }' | $chibicc -o- -S -xc - | grep -q '^; f2$'
check inline

# -idirafter
mkdir -p $tmp/dir1 $tmp/dir2
echo foo > $tmp/dir1/idirafter
echo bar > $tmp/dir2/idirafter
echo "#include \"idirafter\"" | $chibicc -I$tmp/dir1 -I$tmp/dir2 -E -xc - | grep -q foo
check -idirafter
echo "#include \"idirafter\"" | $chibicc -idirafter $tmp/dir1 -I$tmp/dir2 -E -xc - | grep -q bar
check -idirafter

# -fcommon
echo 'int foo;' | $chibicc -S -o- -xc - | grep -q '^;@comm foo'
check '-fcommon (default)'

echo 'int foo;' | $chibicc -fcommon -S -o- -xc - | grep -q '^;@comm foo'
check '-fcommon'

# -fno-common
echo 'int foo;' | $chibicc -fno-common -S -o- -xc - | grep -q '^foo:'
check '-fno-common'

# -include
echo foo > $tmp/out.h
echo bar | $chibicc -include $tmp/out.h -E -o- -xc - | grep -q -z 'foo.*bar'
check -include
echo NULL | $chibicc -Iinclude -include stdio.h -E -o- -xc - | grep -q 0
check -include

# -x
echo 'int x;' | $chibicc -c -xc -o $tmp/foo.o -
check -xc
echo 'x:' | $chibicc -c -x assembler -o $tmp/foo.o -
check '-x assembler'

echo 'int x;' > $tmp/foo.c
$chibicc -c -x assembler -x none -o $tmp/foo.o $tmp/foo.c
check '-x none'

# -E
echo foo | $chibicc -E - | grep -q foo
check -E

# .a and .so files are not supported
echo 'void foo() {}' | $chibicc -c -xc -o $tmp/foo.o -
ar rcs $tmp/foo.a $tmp/foo.o
echo 'void foo(); int main() { foo(); }' > $tmp/main.c
! $chibicc -o $tmp/foo $tmp/main.c $tmp/foo.a 2> $tmp/err && grep -q 'not supported' $tmp/err
check '.a'

touch $tmp/foo.so
! $chibicc -o $tmp/foo $tmp/main.c $tmp/foo.so 2> $tmp/err && grep -q 'dynamic linking is not supported' $tmp/err
check '.so'

$chibicc -hashmap-test
check 'hashmap'

# -M
echo '#include "out2.h"' > $tmp/out.c
echo '#include "out3.h"' >> $tmp/out.c
touch $tmp/out2.h $tmp/out3.h
$chibicc -M -I$tmp $tmp/out.c | grep -q -z '^out.o: .*/out\.c .*/out2\.h .*/out3\.h'
check -M

# -MF
$chibicc -MF $tmp/mf -M -I$tmp $tmp/out.c
grep -q -z '^out.o: .*/out\.c .*/out2\.h .*/out3\.h' $tmp/mf
check -MF

# -MP
$chibicc -MF $tmp/mp -MP -M -I$tmp $tmp/out.c
grep -q '^.*/out2.h:' $tmp/mp
check -MP
grep -q '^.*/out3.h:' $tmp/mp
check -MP

# -MT
$chibicc -MT foo -M -I$tmp $tmp/out.c | grep -q '^foo:'
check -MT
$chibicc -MT foo -MT bar -M -I$tmp $tmp/out.c | grep -q '^foo bar:'
check -MT

# -MD
echo '#include "out2.h"' > $tmp/md2.c
echo '#include "out3.h"' > $tmp/md3.c
(cd $tmp; $OLDPWD/$chibicc -c -MD -I. md2.c md3.c)
grep -q -z '^md2.o:.* md2\.c .* ./out2\.h' $tmp/md2.d
check -MD
grep -q -z '^md3.o:.* md3\.c .* ./out3\.h' $tmp/md3.d
check -MD

$chibicc -c -MD -MF $tmp/md-mf.d -I. $tmp/md2.c
grep -q -z '^md2.o:.*md2\.c .*/out2\.h' $tmp/md-mf.d
check -MD

! $chibicc -fPIC -c -o $tmp/foo.o $tmp/main.c 2> $tmp/err && grep -q 'dynamic linking is not supported' $tmp/err
check -fPIC

# #include_next
mkdir -p $tmp/next1 $tmp/next2 $tmp/next3
echo '#include "file1.h"' > $tmp/file.c
echo '#include_next "file1.h"' > $tmp/next1/file1.h
echo '#include_next "file2.h"' > $tmp/next2/file1.h
echo 'foo' > $tmp/next3/file2.h
$chibicc -I$tmp/next1 -I$tmp/next2 -I$tmp/next3 -E $tmp/file.c | grep -q foo
check '#include_next'

# -static
echo 'extern int bar; int foo() { return bar; }' > $tmp/foo.c
echo 'int foo(); int bar=3; int main() { return foo(); }' > $tmp/bar.c
$chibicc -static -o $tmp/foo $tmp/foo.c $tmp/bar.c
check -static
$run $tmp/foo
[ "$?" = 3 ]
check -static

# -shared, -l, -Wl, and -Xlinker
! $chibicc -shared -o $tmp/foo.so $tmp/foo.c $tmp/bar.c 2> $tmp/err && grep -q 'dynamic linking is not supported' $tmp/err
check -shared
! $chibicc -o $tmp/foo $tmp/bar.c -L$tmp -lfoobar 2> $tmp/err && grep -q 'no libraries' $tmp/err
check -L
! $chibicc -Wl,-z,muldefs -o $tmp/foo $tmp/bar.c 2> $tmp/err && grep -q 'not supported' $tmp/err
check -Wl,
! $chibicc -Xlinker -z -o $tmp/foo $tmp/bar.c 2> $tmp/err && grep -q 'not supported' $tmp/err
check -Xlinker

# Floating point is rejected
echo 'double f(double x) { return x * 2; }' > $tmp/fp.c
! $chibicc -S -o $tmp/fp.s $tmp/fp.c 2> $tmp/err && grep -q 'floating-point is not supported' $tmp/err
check 'floating point'
echo 'int g = (int)2.5; int main() { return sizeof(double) + g; }' > $tmp/fp.c
$chibicc -o $tmp/foo $tmp/fp.c && $run $tmp/foo
[ "$?" = 10 ]
check 'floating point constant expressions'

# Linking
echo 'int undefined_fn(); int main() { return undefined_fn(); }' > $tmp/undef.c
! $chibicc -o $tmp/foo $tmp/undef.c 2> $tmp/err && grep -q "undefined reference to 'undefined_fn'" $tmp/err
check 'undefined reference'

echo 'int dup = 1;' > $tmp/dup1.c
echo 'int dup = 2; int main() { return dup; }' > $tmp/dup2.c
! $chibicc -o $tmp/foo $tmp/dup1.c $tmp/dup2.c 2> $tmp/err && grep -q "multiple definition of 'dup'" $tmp/err
check 'multiple definition'

echo 'int common_var; int a_rather_long_function_name(int x) { static int s = 10; return x + s + common_var; }' > $tmp/long1.c
echo 'int common_var; static int s = 20; int a_rather_long_function_name(int); int main() { common_var = 7; return a_rather_long_function_name(5) + s; }' > $tmp/long2.c
$chibicc -o $tmp/foo $tmp/long1.c $tmp/long2.c && $run $tmp/foo
[ "$?" = 42 ]
check 'long names, statics and commons across units'

echo 'int x;' | $chibicc -c -xc -o $tmp/obj1.o -
echo 'extern int x; int main() { x = 5; return x; }' | $chibicc -c -xc -o $tmp/obj2.o -
$chibicc -o $tmp/foo $tmp/obj1.o $tmp/obj2.o && $run $tmp/foo
[ "$?" = 5 ]
check 'separate compilation'

# Freestanding programs (-nostdlib): main's result or 128 + the exception
# number is the halt code, unless the program provides exit/__acr_trap
echo 'int main(void) { return 5; }' > $tmp/fs.c
$chibicc -nostdlib -o $tmp/foo $tmp/fs.c && $run $tmp/foo
[ "$?" = 5 ]
check '-nostdlib'
echo 'int zero; int main(void) { return 10 / zero; }' > $tmp/fs.c
$chibicc -nostdlib -o $tmp/foo $tmp/fs.c && $run $tmp/foo
[ "$?" = 136 ]
check '-nostdlib exception'
printf 'void exit(int s) { asm("  ld ac, 3(ap)\\n  addi ac, ac, 1\\n  hlt"); }\nint main(void) { return 5; }\n' > $tmp/fs.c
$chibicc -nostdlib -o $tmp/foo $tmp/fs.c && $run $tmp/foo
[ "$?" = 6 ]
check '-nostdlib with exit'

echo OK
