MAKEFLAGS += --no-builtin-rules
MAKEFLAGS += --no-builtin-variables

# The selected C compiler.
CC = gcc

# C preprocessor flags.
#
# Use man gcc for more details. On Debian, the GNU FDL license is considered
# non free, and as a result, the gcc-doc package is part of the non-free
# components.

# The -I option is used to add directories to the search list.
CPPFLAGS = -Iinclude -I.

# Generate code for a 32-bits environment.
CPPFLAGS += -m32

# Do not search the standard system directories for header files.
# The kernel is a free standing environment, where no host library can
# work, at least not without (usually heavy) integration work.
CPPFLAGS += -nostdinc

# The -print-file-name=include option prints the directory where the compiler
# headers are located. This directory is normally part of the standard system
# directories for header files, excluded by the -nostdinc option. But the
# C free standing environment still assumes the availability of some headers
# which can be found there.
CPPFLAGS += -I$(shell $(CC) -print-file-name=include)

# C flags.
#
# Use man gcc for more details. On Debian, the GNU FDL license is considered
# non free, and as a result, the gcc-doc package is part of the non-free
# components.

# These are common warning options.
CFLAGS = -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes

# Set the language as C99 with GNU extensions.
CFLAGS += -std=gnu99

# Build with optimizations as specified by the -O2 option.
CFLAGS += -O2

# Include debugging symbols, giving inspection tools a lot more debugging
# data to work with, e.g. allowing them to translate between addresses and
# source locations.
CFLAGS += -g

# Define this macro in order to turn off assertions.
#CFLAGS += -DNDEBUG

# Target a free standing environment as defined by C99.
#
# This option tells the compiler that it may not assume a hosted environment,
# e.g. that it cannot assume the availability of the C library.
#
# See ISO/IEC 9899:1999 5.1.2.1 "Freestanding environment" for all the details.
CFLAGS += -ffreestanding

# Disable the generation of extra code for stack protection, as it requires
# additional support in the kernel which is beyond its scope, and may be
# enabled by default on some distributions.
CFLAGS += -fno-stack-protector

# Disable strict aliasing, a C99 rule that states that pointers of different
# types may never refer to the same memory. Strict aliasing may provide
# performance improvements for some rare cases but may also cause weird bugs
# when casting pointers.
CFLAGS += -fno-strict-aliasing

# Force all uninitialized global variables into a data section instead of
# generating them as "common blocks". If multiple definitions of the same
# global variable are made, this option will make the link fail.
CFLAGS += -fno-common

# Linker flags.
#
# These are also GCC options, so use man gcc for more details. On Debian,
# the GNU FDL license is considered non free, and as a result, the gcc-doc
# package is part of the non-free components.

# Link for a 32-bits environment.
LDFLAGS = -m32

# Build a static executable, with no shared library.
LDFLAGS += -static

# Do not use the standard system startup files or libraries when linking.
# The kernel is a free standing environment, where no host library can
# work, at least not without (usually heavy) integration work.
LDFLAGS += -nostdlib

# Disable the generation of a build ID, a feature usually enabled by default
# on many distributions. A build ID is linked in its own special section, so
# disabling it makes the linker script simpler.
LDFLAGS += -Xlinker --build-id=none

# Pass the linker script path to the linker.
LDFLAGS += -Xlinker -T src/kernel.lds

# Link against libgcc. This library is a companion to the compiler and
# adds support for operations required by C99 but that the hardware
# doesn't provide. An example is 64-bits integer additions on a 32-bits
# processor or a 64-bits processor running in 32-bits protected mode.
LIBS = -lgcc

BINARY = x1

SOURCES = \
	src/boot_asm.S \
	src/boot.c \
	src/cpu.c \
	src/cpu_asm.S \
	src/i8254.c \
	src/i8259.c \
	src/io_asm.S \
	src/main.c \
	src/mem.c \
	src/mutex.c \
	src/panic.c \
	src/printf.c \
	src/string.c \
	src/thread_asm.S \
	src/thread.c \
	src/uart.c

SOURCES += \
	lib/cbuf.c
	lib/fmt.c

OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SOURCES)))

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINARY) $(OBJECTS)

# Making all sources phony means that make will always consider them and
# the targets using them as dependencies as obsolete. This basically forces
# make to rebuild all files, all the time. It's obviously not the best
# way to compile, but it's simple and reliable.
#
# The modern approach is to make compilation incremental, by generating
# dependency Makefiles with the compiler, which are then included in the
# main Makefile so that a target may only be rebuilt if any of its
# dependencies is newer. Check the X15 project [1] for an example of this
# technique.
#
# [1] https://git.sceen.net/rbraun/x15.git/
.PHONY: clean $(SOURCES)
