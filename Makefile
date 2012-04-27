
CC=gcc -pipe -c -Wall -Wextra -pedantic -ansi -std=c99 -Wshadow -Wpointer-arith\
 -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes\
 -Wno-implicit-function-declaration -Wno-deprecated-declarations
NVCC=nvcc
LD=gcc -pipe
CFLAGS=`pkg-config --silence-errors --cflags libxml-2.0 libavformat libswscale` -O3
LDFLAGS=`pkg-config --silence-errors --libs libxml-2.0 libavformat libswscale` -lcudart -s

all: cellular

cellular: main.o automaton.o automaton.co video.o ass.o
	$(LD) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.co: %.cu
	$(NVCC) -c $(CFLAGS) $< -o $@

clean:
	rm -fr *.o *.co *core*

.PHONY: clean

