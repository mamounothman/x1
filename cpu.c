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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <error.h>
#include <macros.h>
#include <printf.h>
#include <thread.h>

#define CPU_SEG_DATA_RW         0x00000200
#define CPU_SEG_CODE_RX         0x00000900
#define CPU_SEG_S               0x00001000
#define CPU_SEG_P               0x00008000
#define CPU_SEG_DB              0x00400000
#define CPU_SEG_G               0x00800000

#define CPU_IDT_SIZE 256

struct cpu_seg_desc {
    uint32_t low;
    uint32_t high;
};

/*
 * This structure is packed to prevent any holes between limit and base.
 */
struct cpu_pseudo_desc {
    uint16_t limit;
    uint32_t base;
} __packed;

struct cpu_intr_frame {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t vector;
    uint32_t error;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
};

struct cpu_intr_handler {
    cpu_intr_handler_fn_t fn;
    void *arg;
};

/*
 * TODO Reference alignment recommendation.
 */
static struct cpu_seg_desc cpu_gdt[CPU_GDT_SIZE] __aligned(8);

static struct cpu_seg_desc cpu_idt[CPU_IDT_SIZE] __aligned(8);

static struct cpu_intr_handler cpu_intr_handlers[16]; /* TODO Macros */

void cpu_load_gdt(const struct cpu_pseudo_desc *desc);
void cpu_load_idt(const struct cpu_pseudo_desc *desc);
void cpu_intr_main(struct cpu_intr_frame *frame);

/*
 * Low level interrupt service routines.
 */
void cpu_isr_general_protection(void);
void cpu_isr_32(void);
void cpu_isr_33(void);
void cpu_isr_34(void);
void cpu_isr_35(void);
void cpu_isr_36(void);
void cpu_isr_37(void);
void cpu_isr_38(void);
void cpu_isr_39(void);
void cpu_isr_40(void);
void cpu_isr_41(void);
void cpu_isr_42(void);
void cpu_isr_43(void);
void cpu_isr_44(void);
void cpu_isr_45(void);
void cpu_isr_46(void);
void cpu_isr_47(void);

uint32_t
cpu_intr_save(void)
{
    uint32_t eflags;

    eflags = cpu_get_eflags();
    cpu_intr_disable();
    return eflags;
}

void
cpu_intr_restore(uint32_t eflags)
{
    cpu_set_eflags(eflags);
}

bool
cpu_intr_enabled(void)
{
    uint32_t eflags;

    eflags = cpu_get_eflags();
    return eflags & CPU_EFL_IF;
}

void
cpu_halt(void)
{
    for (;;) {
        cpu_idle();
    }
}

static void
cpu_default_intr_handler(void)
{
    printf("cpu: error: unhandled interrupt\n");
    cpu_halt();
}

static void
cpu_seg_desc_init_null(struct cpu_seg_desc *desc)
{
    desc->low = 0;
    desc->high = 0;
}

static void
cpu_seg_desc_init_code(struct cpu_seg_desc *desc)
{
    /*
     * Base: 0
     * Limit: 0xffffffff
     * Privilege level: 0 (most privileged)
     */
    desc->low = 0xffff;
    desc->high = CPU_SEG_G
                 | CPU_SEG_DB
                 | (0xf << 16)
                 | CPU_SEG_P
                 | CPU_SEG_S
                 | CPU_SEG_CODE_RX;
}

static void
cpu_seg_desc_init_data(struct cpu_seg_desc *desc)
{
    /*
     * Base: 0
     * Limit: 0xffffffff
     * Privilege level: 0 (most privileged)
     */
    desc->low = 0xffff;
    desc->high = CPU_SEG_G
                 | CPU_SEG_DB
                 | (0xf << 16)
                 | CPU_SEG_P
                 | CPU_SEG_S
                 | CPU_SEG_DATA_RW;
}

static void
cpu_seg_desc_init_intr_gate(struct cpu_seg_desc *desc,
                            void (*handler)(void))
{
    desc->low = (CPU_GDT_SEL_CODE << 16)
                | (((uint32_t)handler) & 0xffff);
    desc->high = (((uint32_t)handler) & 0xffff0000)
                 | CPU_SEG_P
                 | 0xe00;
}

static void
cpu_pseudo_desc_init(struct cpu_pseudo_desc *desc,
                     const void *addr, size_t size)
{
    assert(size <= 0x10000);
    desc->limit = size - 1;
    desc->base = (uint32_t)addr;
}

static struct cpu_seg_desc *
cpu_get_gdt_entry(size_t selector)
{
    size_t index;

    /*
     * The first 3 bits are the TI and RPL bits
     *
     * See Volume 3: System programming guide, "3.4.2 Segment Selectors".
     */
    index = selector >> 3;
    assert(index < ARRAY_SIZE(cpu_gdt));
    return &cpu_gdt[index];
}

static void
cpu_intr_handler_init(struct cpu_intr_handler *handler)
{
    handler->fn = NULL;
}

static struct cpu_intr_handler *
cpu_lookup_intr_handler(unsigned int irq)
{
    assert(irq < ARRAY_SIZE(cpu_intr_handlers));
    return &cpu_intr_handlers[irq];
}

static int
cpu_intr_handler_set_fn(struct cpu_intr_handler *handler,
                        cpu_intr_handler_fn_t fn, void *arg)
{
    if (handler->fn != NULL) {
        return ERROR_AGAIN;
    }

    handler->fn = fn;
    handler->arg = arg;
    return 0;
}

static void
cpu_setup_gdt(void)
{
    struct cpu_pseudo_desc pseudo_desc;

    cpu_seg_desc_init_null(cpu_get_gdt_entry(CPU_GDT_SEL_NULL));
    cpu_seg_desc_init_code(cpu_get_gdt_entry(CPU_GDT_SEL_CODE));
    cpu_seg_desc_init_data(cpu_get_gdt_entry(CPU_GDT_SEL_DATA));

    cpu_pseudo_desc_init(&pseudo_desc, cpu_gdt, sizeof(cpu_gdt));
    cpu_load_gdt(&pseudo_desc);
}

static void
cpu_setup_idt(void)
{
    struct cpu_pseudo_desc pseudo_desc;

    for (size_t i = 0; i < ARRAY_SIZE(cpu_intr_handlers); i++) {
        cpu_intr_handler_init(cpu_lookup_intr_handler(i));
    }

    for (size_t i = 0; i < ARRAY_SIZE(cpu_idt); i++) {
        cpu_seg_desc_init_intr_gate(&cpu_idt[i], cpu_default_intr_handler);
    }

    cpu_seg_desc_init_intr_gate(&cpu_idt[CPU_IDT_VECT_GP],
                                cpu_isr_general_protection);
    cpu_seg_desc_init_intr_gate(&cpu_idt[32], cpu_isr_32);
    cpu_seg_desc_init_intr_gate(&cpu_idt[33], cpu_isr_33);
    cpu_seg_desc_init_intr_gate(&cpu_idt[34], cpu_isr_34);
    cpu_seg_desc_init_intr_gate(&cpu_idt[35], cpu_isr_35);
    cpu_seg_desc_init_intr_gate(&cpu_idt[36], cpu_isr_36);
    cpu_seg_desc_init_intr_gate(&cpu_idt[37], cpu_isr_37);
    cpu_seg_desc_init_intr_gate(&cpu_idt[38], cpu_isr_38);
    cpu_seg_desc_init_intr_gate(&cpu_idt[39], cpu_isr_39);
    cpu_seg_desc_init_intr_gate(&cpu_idt[40], cpu_isr_40);
    cpu_seg_desc_init_intr_gate(&cpu_idt[41], cpu_isr_41);
    cpu_seg_desc_init_intr_gate(&cpu_idt[42], cpu_isr_42);
    cpu_seg_desc_init_intr_gate(&cpu_idt[43], cpu_isr_43);
    cpu_seg_desc_init_intr_gate(&cpu_idt[44], cpu_isr_44);
    cpu_seg_desc_init_intr_gate(&cpu_idt[45], cpu_isr_45);
    cpu_seg_desc_init_intr_gate(&cpu_idt[46], cpu_isr_46);
    cpu_seg_desc_init_intr_gate(&cpu_idt[47], cpu_isr_47);

    cpu_pseudo_desc_init(&pseudo_desc, cpu_idt, sizeof(cpu_idt));
    cpu_load_idt(&pseudo_desc);
}

void
cpu_intr_main(struct cpu_intr_frame *frame)
{
    struct cpu_intr_handler *handler;

    assert(!cpu_intr_enabled());

    handler = cpu_lookup_intr_handler(frame->vector - 32); /* TODO Macros */

    if (!handler || !handler->fn) {
        printf("cpu: error: invalid handler for vector %u\n", frame->vector);
        return;
    }

    handler->fn(handler->arg);
    thread_yield_if_needed();
}

int
cpu_intr_register(unsigned int irq, cpu_intr_handler_fn_t fn, void *arg)
{
    struct cpu_intr_handler *handler;

    handler = cpu_lookup_intr_handler(irq);
    return cpu_intr_handler_set_fn(handler, fn, arg);
}

void
cpu_setup(void)
{
    cpu_setup_gdt();
    cpu_setup_idt();
}
