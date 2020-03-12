TESTS := syscall syscall2 noop call var ro rw def static
OUTS := $(addprefix exec_, $(TESTS)) \
	$(addsuffix .o, $(addprefix rel_, $(TESTS)))
CC := gcc
CFLAGS := -O2 -fno-common

all: $(OUTS)

exec_%: exec_%.c
	gcc -O2 -no-pie -fno-pie -o $@ $<

exec_static: exec_var.c
	gcc -O2 -static -no-pie -fno-pie -o $@ $<

rel_static.o: rel_var.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OUTS)
