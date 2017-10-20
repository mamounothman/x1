MAKEFLAGS += --no-builtin-rules
MAKEFLAGS += --no-builtin-variables

CC = gcc

CPPFLAGS = \
	-I. \
        -m32 \
        -nostdinc

# TODO Explain
CPPFLAGS += -I$(shell $(CC) -print-file-name=include)

CFLAGS = \
        -Wall -Wmissing-prototypes -Wstrict-prototypes \
        -O2 -std=gnu99 -g \
        -ffreestanding \
        -fno-strict-aliasing

LDFLAGS = \
        -m32 \
        -static \
        -nostdlib \
        -lgcc \
        -Xlinker --build-id=none \
        -Xlinker -T kernel.lds

BINARY = x1
SOURCES = \
        boot.S \
        fmt.c \
        io.S \
        main.c \
        printf.c \
        string.c \
        uart.c

OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SOURCES)))

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINARY) $(OBJECTS)

.PHONY: clean $(SOURCES)
