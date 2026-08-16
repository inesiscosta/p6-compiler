# Builds Tetris with the p6 compiler one directory up.
#   tetris.p6 --p6--> tetris.asm --yasm--> tetris.o --ld + libp6 (key, clock, random) + librts--> tetris
# The result is a static 32-bit Linux executable.

ROOT        = $(HOME)/compiladores/root
CDK_LIB_DIR = $(ROOT)/usr/lib

P6      = ../p6
RUNTIME = ../runtime
AS      = yasm
ASFLAGS = -felf32

all: tetris

$(P6):
	$(MAKE) -C ..

$(RUNTIME)/libp6.a $(RUNTIME)/libp6-det.a:
	$(MAKE) -C $(RUNTIME)

tetris.asm: tetris.p6 $(P6)
	$(P6) --target asm $< 2>p6.log || { tail -5 p6.log; exit 1; }

tetris.o: tetris.asm
	$(AS) $(ASFLAGS) $< -o $@

tetris: tetris.o $(RUNTIME)/libp6.a
	ld -m elf_i386 -o $@ tetris.o -L$(RUNTIME) -lp6 -L$(CDK_LIB_DIR) -lrts

# A reproducible build for testing emulators: fake clock, fixed seed, keys scripted on stdin.
tetris-det: tetris.o $(RUNTIME)/libp6-det.a
	ld -m elf_i386 -o $@ tetris.o -L$(RUNTIME) -lp6-det -L$(CDK_LIB_DIR) -lrts

run: tetris
	./tetris

clean:
	$(RM) tetris tetris-det tetris.asm tetris.o

.PHONY: all run clean
