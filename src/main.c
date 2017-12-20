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

#include <stdbool.h>
#include <stdio.h>

#include <lib/macros.h>
#include <lib/shell.h>

#include "cpu.h"
#include "i8254.h"
#include "i8259.h"
#include "mem.h"
#include "panic.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"

/*
 * XXX The Clang compiler apparently doesn't like the lack of prototype for
 * the main function in free standing mode.
 */
void main(void);

/* TODO External stopwatch module */
/* TODO Mutual exclusion */
static struct timer sw_timer;
static unsigned long sw_time;
static bool sw_timer_scheduled;

static void
sw_timer_run(void *arg)
{
    unsigned long ticks;

    (void)arg;

    if (!sw_timer_scheduled) {
        return;
    }

    sw_time++;
    ticks = timer_get_time(&sw_timer);

    if ((ticks % THREAD_SCHED_FREQ) == 0) {
        printf("%lu\n", sw_time);
    }

    /* TODO Discuss drift */
    timer_schedule(&sw_timer, timer_get_time(&sw_timer) + 1);
}

static void
sw_start(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (sw_timer_scheduled) {
        printf("sw: error: timer already scheduled\n");
        return;
    }

    sw_time = 0;
    sw_timer_scheduled = true;

    /* TODO Discuss resolution */
    timer_schedule(&sw_timer, timer_now() + 1);
}

static void
sw_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sw_timer_scheduled = false;
}

static void
sw_resume(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sw_timer_scheduled = true;
    timer_schedule(&sw_timer, timer_now() + 1);
}

static void
sw_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("sw_time: %lu\n", sw_time);
}

static struct shell_cmd shell_cmds[] = {
    SHELL_CMD_INITIALIZER("sw_start", sw_start,
        "sw_start",
        "start the stopwatch"),
    SHELL_CMD_INITIALIZER("sw_stop", sw_stop,
        "sw_stop",
        "stop the stopwatch"),
    SHELL_CMD_INITIALIZER("sw_resume", sw_resume,
        "sw_resume",
        "resume the stopwatch"),
    SHELL_CMD_INITIALIZER("sw_read", sw_read,
        "sw_read",
        "read the stopwatch time"),
};

/*
 * This function is the main entry point for C code. It's called from
 * assembly code in the boot module, very soon after control is passed
 * to the kernel.
 */
void
main(void)
{
    int error;

    thread_bootstrap();
    cpu_setup();
    i8259_setup();
    i8254_setup();
    uart_setup();
    mem_setup();
    thread_setup();
    timer_setup();
    shell_setup();

    timer_init(&sw_timer, sw_timer_run, NULL);
    sw_timer_scheduled = false;

    for (size_t i = 0; i < ARRAY_SIZE(shell_cmds); i++) {
        error = shell_cmd_register(&shell_cmds[i]);

        if (error) {
            panic("unable to register shell command");
        }
    }

    printf("X1 " QUOTE(VERSION) "\n\n");

    thread_enable_scheduler();

    /* Never reached */
}
