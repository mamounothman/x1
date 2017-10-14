CC = gcc
CFLAGS = \
        -Wall -Wmissing-prototypes -Wstrict-prototypes \
        -D _GNU_SOURCE -O2 -std=gnu11 -ggdb3 \
        -fno-strict-aliasing

BINARY = hello
OBJECTS = main.o

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINARY) $(OBJECTS)

.PHONY: clean
