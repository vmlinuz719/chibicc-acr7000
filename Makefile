CFLAGS=-std=c11 -g -fno-common -Wall -Wno-switch

# chibicc for the ACR-7000. The compiler itself is built for the host;
# it generates code for the ACR-7000 and uses the emulator project's
# assembler (asm2) to produce paper tape images.
ACR7000=../acr7000

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: chibicc asm2 acrrun acr/libc.o

chibicc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): chibicc.h

# The ACR-7000 assembler, built from the emulator's sources.
asm2: $(ACR7000)/not_vibe_code/asm2.c
	$(CC) -O2 -w -o $@ $<

# Headless runner: the emulator core with a batch driver instead of the
# interactive monitor (see acr/acrrun.c).
ACRRUN_OBJS=acr/acrrun.o acr/acr7000-cpu.o acr/acr7000-fpu.o

acrrun: $(ACRRUN_OBJS)
	$(CC) -o $@ $^ -lpthread

acr/acrrun.o: acr/acrrun.c
	$(CC) -O2 -Wall -I$(ACR7000)/include -c -o $@ $<

acr/acr7000-cpu.o: $(ACR7000)/cpu.c $(wildcard $(ACR7000)/include/*.h)
	$(CC) -O2 -w -I$(ACR7000)/include -I$(ACR7000) -Dmain=acr7k_monitor_main -c -o $@ $<

acr/acr7000-fpu.o: $(ACR7000)/fpu.c
	$(CC) -O2 -w -I$(ACR7000)/include -c -o $@ $<

# The C library, compiled by chibicc itself.
acr/libc.o: chibicc acr/libc.c $(wildcard acr/include/*.h) $(wildcard include/*.h)
	./chibicc -c -o $@ acr/libc.c

# Run chibicc's test suite (adapted where needed, see acr/test) on the
# emulator.
test: all
	acr/test/run.sh
	acr/test/run.sh acr7000
	acr/test/driver.sh ./chibicc

# Regenerate the adapted test files from test/*.c.
test-adapt:
	python3 acr/test/adapt.py

clean:
	rm -rf chibicc asm2 acrrun tmp* $(ACRRUN_OBJS) acr/libc.o
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.PHONY: all test test-adapt clean
