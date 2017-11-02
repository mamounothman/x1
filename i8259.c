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

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <assert.h>
#include <cpu.h>
#include <i8259.h>
#include <io.h>
#include <macros.h>

#define I8259_IRQ_CASCADE   2
#define I8259_NR_IRQS       8

#define I8259_MASTER_CMD    0x20
#define I8259_MASTER_DAT    0x21
#define I8259_SLAVE_CMD     0xA0
#define I8259_SLAVE_DAT     0xA1

#define I8259_ICW1_ICW4     0x01
#define I8259_ICW1_INIT     0x10
 
#define I8259_ICW4_8086     0x01

#define I8259_PIC_ID_MASTER 0
#define I8259_PIC_ID_SLAVE  1
#define I8259_NR_PICS       2

struct i8259_pic {
    uint16_t cmd_port;
    uint16_t data_port;
    uint8_t mask;
    bool master;
};

static struct i8259_pic i8259_pics[] = {
    [I8259_PIC_ID_MASTER] = {
        .cmd_port = I8259_MASTER_CMD,
        .data_port = I8259_MASTER_DAT,
        .mask = 0xff,
        .master = true,
    },
    [I8259_PIC_ID_SLAVE] = {
        .cmd_port = I8259_SLAVE_CMD,
        .data_port = I8259_SLAVE_DAT,
        .mask = 0xff,
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
i8259_pic_apply_mask(const struct i8259_pic *pic)
{
    io_write(pic->data_port, pic->mask);
}

static void
i8259_pic_enable_irq(struct i8259_pic *pic, unsigned int irq)
{
    if (!pic->master) {
        assert(irq >= I8259_NR_IRQS);
        irq -= I8259_NR_IRQS;
    }

    assert(irq < I8259_NR_IRQS);

    pic->mask &= ~(1 << irq);
    i8259_pic_apply_mask(pic);
}

static void
i8259_pic_disable_irq(struct i8259_pic *pic, unsigned int irq)
{
    if (!pic->master) {
        assert(irq >= I8259_NR_IRQS);
        irq -= I8259_NR_IRQS;
    }

    assert(irq < I8259_NR_IRQS);

    pic->mask |= (1 << irq);
    i8259_pic_apply_mask(pic);
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
    i8259_pic_apply_mask(master);
    i8259_pic_apply_mask(slave);
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

