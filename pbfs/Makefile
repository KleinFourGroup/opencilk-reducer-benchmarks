CC=gcc
CFLAGS= -O3
CILK=/home/matt/opencilk/build/bin/clang++
CILKFLAGS= -O3 -fopencilk
LDFLAGS= -L$(CURDIR)
AR=ar

all: bfs

bfs : bfs.cpp graph.cpp graph.h bag.cpp bag.h util.h ../ktiming.c ../ktiming.h Makefile
	$(CILK) $(CILKFLAGS) $@.cpp $(LDFLAGS) -o $@

clean :
	rm -f bfs *~
