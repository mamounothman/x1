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
#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <i8259.h>
#include <io.h>
#include <printf.h>
#include <uart.h>
#include <thread.h>

#define UART_BAUD_RATE          115200

#define UART_CLOCK              115200
#define UART_DIVISOR            (UART_CLOCK / UART_BAUD_RATE)

#define UART_IRQ                4

#define UART_IER_DATA           0x1

#define UART_LCR_8BITS          0x3
#define UART_LCR_STOP1          0
#define UART_LCR_PARITY_NONE    0
#define UART_LCR_DLAB           0x80

#define UART_COM1_PORT          0x3F8
#define UART_REG_DAT            0
#define UART_REG_DIVL           0
#define UART_REG_IER            1
#define UART_REG_DIVH           1
#define UART_REG_LCR            3

static void
uart_intr_handler(void *arg)
{
    uint8_t byte;

    (void)arg;

    i8259_irq_eoi(UART_IRQ);

    byte = io_read(UART_COM1_PORT + UART_REG_DAT);

    printf("%c", byte);

    if (byte == '\r') {
        printf("\n");
    }
}

void
uart_setup(void)
{
    io_write(UART_COM1_PORT + UART_REG_LCR, UART_LCR_DLAB);
    io_write(UART_COM1_PORT + UART_REG_DIVL, UART_DIVISOR);
    io_write(UART_COM1_PORT + UART_REG_DIVH, UART_DIVISOR >> 8);
    io_write(UART_COM1_PORT + UART_REG_LCR, UART_LCR_8BITS | UART_LCR_STOP1
                                            | UART_LCR_PARITY_NONE);

    /* TODO Explain order */
    cpu_intr_register(UART_IRQ, uart_intr_handler, NULL);
    i8259_irq_enable(UART_IRQ);
    io_write(UART_COM1_PORT + UART_REG_IER, UART_IER_DATA);
}

void
uart_write(uint8_t byte)
{
    assert(!cpu_intr_enabled());
    assert(!thread_preempt_enabled());

    io_write(UART_COM1_PORT + UART_REG_DAT, byte);
}
