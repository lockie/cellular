
CC=gcc -pipe -c -Wall -Wextra -pedantic
LD=gcc -pipe
CFLAGS=`pkg-config --silence-errors --cflags libxml-2.0 libavformat libswscale` -O3
LDFLAGS=`pkg-config --silence-errors --libs libxml-2.0 libavformat libswscale` -s

all: cellular

cellular: main.o automaton.o video.o
	$(LD) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -fr *.o *core*

.PHONY: clean
