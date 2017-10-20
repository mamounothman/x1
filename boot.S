/*
 * Copyright (c) 2017 Richard Braun.
 * Copyright (c) 2017 Jerko Lenstra.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
