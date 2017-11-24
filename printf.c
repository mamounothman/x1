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

#include <stdarg.h>
#include <stdint.h>

#include <cpu.h>
#include <fmt.h>
#include <printf.h>
#include <uart.h>

/* TODO Discuss stack vs static and size */
#define PRINTF_BUFFER_SIZE 1024

static char printf_buffer[PRINTF_BUFFER_SIZE];

int
printf(const char *format, ...)
{
    va_list ap;
    int length;

    va_start(ap, format);
    length = vprintf(format, ap);
    va_end(ap);

    return length;
}

int
vprintf(const char *format, va_list ap)
{
    uint32_t eflags;
    int length;

    eflags = cpu_intr_save();

    length = fmt_vsnprintf(printf_buffer, sizeof(printf_buffer), format, ap);

    for (const char *ptr = printf_buffer; *ptr != '\0'; ptr++) {
        /* TODO Discuss cast */
        uart_write((uint8_t)*ptr);
    }

    cpu_intr_restore(eflags);

    return length;
}
