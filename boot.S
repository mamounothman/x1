#include <defs.h>

/* TODO Refer to the multiboot specification */
#define BOOT_HDR_MAGIC  0x1BADB002
#define BOOT_HDR_CHECK  0x2BADB002
#define BOOT_HDR_FLAGS  0x0

/* TODO Explain allocatbl */
.section .hdr, "a"

/* Generate code for i386 */
.code32

/* TODO Detail two's complement subtraction */
.int BOOT_HDR_MAGIC
.int BOOT_HDR_FLAGS
.int -(BOOT_HDR_FLAGS + BOOT_HDR_MAGIC)

.section .text, "ax"

_start:
  cmp $BOOT_HDR_CHECK, %eax
  jnz .
  mov $stack, %esp
  add $STACK_SIZE, %esp
  jmp main

  /* Not reached */
  jmp .
