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
 *
 *
 * IRQ means Interrupt ReQuest. They're used by external hardware to signal
 * the CPU, and in turn the OS, that an external event has happened and
 * requires processing. The usual model is show in the image at
 * https://upload.wikimedia.org/wikipedia/commons/thumb/1/15/PIC_Hardware_interrupt_path.svg/300px-PIC_Hardware_interrupt_path.svg.png.
 *
 * This driver implements IRQ handling on the Intel 8259 PIC. The IBM PC/AT
 * actually uses 2 of these PICs for external interrupt handling, as shown
 * in https://masherz.files.wordpress.com/2010/08/217.jpg. The public
 * interface completely hides this detail and considers all given IRQs
 * as logical indexes, used to find the corresponding PIC (master or slave)
 * and the local IRQ on that PIC.
 *
 * 8259 datasheet :
 *   https://pdos.csail.mit.edu/6.828/2010/readings/hardware/8259A.pdf
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <assert.h>
#include <cpu.h>
#include <i8259.h>
#include <io.h>
#include <macros.h>

#define I8259_IRQ_CASCADE   2        /* IRQ used for cascading on the master */
#define I8259_NR_IRQS       8

/*
 * Initialization Control Word 1 bits.
 */
#define I8259_ICW1_ICW4     0x01    /* State that a 4th ICW will be sent */
#define I8259_ICW1_INIT     0x10    /* This bit must be set */

/*
 * Initialization Control Word 4 bits.
 */
#define I8259_ICW4_8086     0x01    /* 8086 mode, as x86 is still compatible
                                       with the old 8086 processor */

enum {
    I8259_PIC_ID_MASTER,
    I8259_PIC_ID_SLAVE,
    I8259_NR_PICS
};

/*
 * This structure is used as a class defining a PIC object.
 *
 * Using functions as methods, this allows using some of the patterns
 * of object-oriented programming, such as encapsulation, to isolate
 * and simplify problems, and make code easily reusable.
 */
struct i8259_pic {
    uint16_t cmd_port;      /* Command I/O port of the PIC */
    uint16_t data_port;     /* Data I/O port of the PIC */
    uint8_t imr;            /* Cached value of the IMR register */
    bool master;            /* True if this PIC is the master */
};

/*
 * Static instances of PIC objects.
 */
static struct i8259_pic i8259_pics[] = {
    [I8259_PIC_ID_MASTER] = {
        .cmd_port = 0x20,
        .data_port = 0x21,
        .imr = 0xff,
        .master = true,
    },
    [I8259_PIC_ID_SLAVE] = {
        .cmd_port = 0xa0,
        .data_port = 0xa1,
        .imr = 0xff,
        .master = false,
    },
};

static struct i8259_pic *
i8259_get_pic(unsigned int id)
{
    assert(id < ARRAY_SIZE(i8259_pics));
    return &i8259_pics[id];
}

static struct i8259_pic *
i8259_get_pic_from_irq(unsigned int irq)
{
    if (irq < I8259_NR_IRQS) {
        return i8259_get_pic(I8259_PIC_ID_MASTER);
    } else if (irq < (I8259_NR_IRQS * I8259_NR_PICS)) {
        return i8259_get_pic(I8259_PIC_ID_SLAVE);
    }

    return NULL;
}

static void
i8259_pic_write_cmd(const struct i8259_pic *pic, uint8_t byte)
{
    io_write(pic->cmd_port, byte);
}

static void
i8259_pic_write_data(const struct i8259_pic *pic, uint8_t byte)
{
    io_write(pic->data_port, byte);
}

static void
i8259_pic_apply_imr(const struct i8259_pic *pic)
{
    io_write(pic->data_port, pic->imr);
}

static void
i8259_pic_enable_irq(struct i8259_pic *pic, unsigned int irq)
{
    if (!pic->master) {
        assert(irq >= I8259_NR_IRQS);
        irq -= I8259_NR_IRQS;
    }

    assert(irq < I8259_NR_IRQS);

    pic->imr &= ~(1 << irq);
    i8259_pic_apply_imr(pic);
}

static void
i8259_pic_disable_irq(struct i8259_pic *pic, unsigned int irq)
{
    if (!pic->master) {
        assert(irq >= I8259_NR_IRQS);
        irq -= I8259_NR_IRQS;
    }

    assert(irq < I8259_NR_IRQS);

    pic->imr |= (1 << irq);
    i8259_pic_apply_imr(pic);
}

void
i8259_setup(void)
{
    struct i8259_pic *master, *slave;

    master = i8259_get_pic(I8259_PIC_ID_MASTER);
    slave = i8259_get_pic(I8259_PIC_ID_SLAVE);

    i8259_pic_write_cmd(master, I8259_ICW1_INIT | I8259_ICW1_ICW4);
    i8259_pic_write_cmd(slave, I8259_ICW1_INIT | I8259_ICW1_ICW4);
    i8259_pic_write_data(master, CPU_IDT_VECT_PIC_MASTER);
    i8259_pic_write_data(slave, CPU_IDT_VECT_PIC_SLAVE);
    i8259_pic_write_data(master, 1 << I8259_IRQ_CASCADE);
    i8259_pic_write_data(slave, I8259_IRQ_CASCADE);
    i8259_pic_write_data(master, I8259_ICW4_8086);
    i8259_pic_write_data(slave, I8259_ICW4_8086);

    i8259_pic_enable_irq(master, I8259_IRQ_CASCADE);
    i8259_pic_apply_imr(master);
    i8259_pic_apply_imr(slave);
}

void i8259_irq_enable(unsigned int irq)
{
    struct i8259_pic *pic;

    pic = i8259_get_pic_from_irq(irq);
    assert(pic);
    i8259_pic_enable_irq(pic, irq);
}

void i8259_irq_disable(unsigned int irq)
{
    struct i8259_pic *pic;

    pic = i8259_get_pic_from_irq(irq);
    assert(pic);
    i8259_pic_disable_irq(pic, irq);
}
