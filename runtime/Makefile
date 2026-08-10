# The runtime for P6's key, clock and random built-ins. Link it after the program and before librts:
#   ld -m elf_i386 -o prog prog.o -L runtime -lp6 -L$(CDK_LIB_DIR) -lrts

CC     = gcc
CFLAGS = -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -O2 -Wall

all: libp6.a libp6-det.a

p6.o: p6.c
	$(CC) $(CFLAGS) -c $< -o $@

# A reproducible build for tests: a fake clock, a fixed seed, and keys scripted on stdin.
p6-det.o: p6.c
	$(CC) $(CFLAGS) -DDETERMINISTIC -c $< -o $@

libp6.a: p6.o
	$(AR) rcs $@ $^

libp6-det.a: p6-det.o
	$(AR) rcs $@ $^

clean:
	$(RM) p6.o p6-det.o libp6.a libp6-det.a

.PHONY: all clean
