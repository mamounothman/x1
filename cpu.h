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

#ifndef _CPU_H
#define _CPU_H

/*
 * EFLAGS register flags.
 */
#define CPU_EFL_IF 0x200

/*
 * GDT segment descriptor indexes.
 */
#define CPU_GDT_SEL_NULL    0x00
#define CPU_GDT_SEL_CODE    0x08
#define CPU_GDT_SEL_DATA    0x10
#define CPU_GDT_SIZE    3

/*
 * IDT segment descriptor indexes.
 * Exception and interrupt vectors.
 */
#define CPU_IDT_VECT_PIC_MASTER     32
#define CPU_IDT_VECT_PIC_SLAVE      (CPU_IDT_VECT_PIC_MASTER + 8)

#ifndef __ASSEMBLER__

#include <stdbool.h>
#include <stdint.h>

/*
 * Return the content of the EFLAGS register.
 */
uint32_t cpu_get_eflags(void);

/*
 * Enable/disable interrupts.
 */
void cpu_intr_enable(void);
void cpu_intr_disable(void);

/*
 * Return true if interrupts are enabled.
 *
 * Implies a compiler barrier.
 */
static inline bool
cpu_intr_enabled(void)
{
    uint32_t eflags;

    eflags = cpu_get_eflags();
    return eflags & CPU_EFL_IF;
}

void cpu_setup(void);

#endif /* __ASSEMBLER__ */

#endif /* _CPU_H */