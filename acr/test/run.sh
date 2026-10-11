#!/bin/bash
# Run chibicc's test suite on the ACR-7000 emulator.
#
#   acr/test/run.sh [test names...]
#
# Tests come from test/*.c; where acr/test/<name>.c exists, that adapted
# copy is used instead (expectations changed for 36-bit words and
# sizeof(int) == 1, floating-point cases removed). float.c is skipped:
# floating point is not supported.
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/chibicc-acr-test-XXXXXX)
trap 'rm -rf $tmp' EXIT

tests=("$@")
if [ ${#tests[@]} -eq 0 ]; then
  tests=($(ls test/*.c | xargs -n1 basename | sed 's/\.c$//' | grep -vx float))
fi

pass=0; fail=0
for t in "${tests[@]}"; do
  src=test/$t.c
  [ -f acr/test/$t.c ] && src=acr/test/$t.c

  if ! ./chibicc -Iinclude -Itest -o $tmp/$t.ppt acr/test/common.c $src > $tmp/$t.cc 2>&1; then
    echo "$t: COMPILE FAILED"
    grep -v '^ ' $tmp/$t.cc | grep -v 'truncated$' | head -${SHOW:-5} | sed 's/^/    /'
    fail=$((fail+1))
    continue
  fi

  timeout ${TIMEOUT:-60} ./acrrun -q $tmp/$t.ppt > $tmp/$t.out 2>&1
  status=$?
  if [ $status -eq 0 ] && tail -1 $tmp/$t.out | grep -q '^OK$'; then
    echo "$t: ok ($(grep -c '=>' $tmp/$t.out) assertions)"
    pass=$((pass+1))
  else
    echo "$t: FAILED (status $status)"
    grep -E 'expected but got|exception|assertion|abort' $tmp/$t.out | head -${SHOW:-20} | sed 's/^/    /'
    [ $status -eq 124 ] && echo "    (timed out)"
    fail=$((fail+1))
  fi
done
echo "passed: $pass, failed: $fail"
[ $fail -eq 0 ]
