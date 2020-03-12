#!/bin/bash
make

PROG=${PROG:=./postlinker}
for tst in syscall syscall2 call noop rw ro def var static; do
	echo === Test $tst ===
	${PROG} exec_${tst} rel_${tst}.o patched_${tst} 2>&1 > /dev/null
	./patched_${tst} > tmp.out
	cmp tmp.out ${tst}.out && echo "OK"
done

echo === Test double call ===
${PROG} exec_call rel_call.o tmp 2>&1 > /dev/null
${PROG} tmp rel_call.o tmp2 2>&1 > /dev/null
./tmp2 > tmp.out
cmp tmp.out call2.out && echo OK
