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

#include <cpu.h>

.section .text
.code32

.global cpu_get_eflags
cpu_get_eflags:
 pushf
 pop %eax
 ret

.global cpu_intr_enable
cpu_intr_enable:
 sti
 ret

.global cpu_intr_disable
cpu_intr_disable:
 cli
 ret

.global cpu_load_gdt
cpu_load_gdt:
 mov 4(%esp), %eax
 lgdt (%eax)

 mov $CPU_GDT_SEL_DATA, %eax
 mov %eax, %ds
 mov %eax, %es
 mov %eax, %fs
 mov %eax, %gs
 mov %eax, %ss

 ljmp $CPU_GDT_SEL_CODE, $1f

1:
 ret

.global cpu_load_idt
cpu_load_idt:
 mov 4(%esp), %eax
 lidt (%eax)

 int $0x80

 ret
