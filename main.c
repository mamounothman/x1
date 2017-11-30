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
#include <i8259.h>
#include <mem.h>
#include <panic.h>
#include <printf.h>
#include <thread.h>
#include <uart.h>

static void
test(void *arg)
{
    struct thread *thread;

    (void)arg;

    thread = thread_self();

    for (;;) {
        printf("test: name: %s\n", thread_name(thread));
        thread_yield();
    }
}

/*
 * This function is the main entry point for C code. It's called from
 * assembly code in the boot module, very soon after control is passed
 * to the kernel.
 */
void
main(void)
{
    struct thread *thread;
    int error;

    thread_bootstrap();
    cpu_setup();
    i8259_setup();
    uart_setup();
    mem_setup();
    thread_setup();

    error = thread_create(&thread, test, NULL, "test1", 1024);

    if (error) {
        panic("main: error: unable to create test1 thread");
    }

    error = thread_create(&thread, test, NULL, "test2", 1024);

    if (error) {
        panic("main: error: unable to create test2 thread");
    }

    thread_enable_scheduler();

    /* Never reached */
}
