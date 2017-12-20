/*
 * Copyright (c) 2017 Richard Braun.
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
#include <stddef.h>
#include <stdio.h>

#include <lib/macros.h>
#include <lib/shell.h>

#include "mutex.h"
#include "panic.h"
#include "sw.h"
#include "thread.h"
#include "timer.h"

/*
 * Display interval, in seconds.
 */
#define SW_DISPLAY_INTERVAL 5

static struct mutex sw_mutex;
static struct timer sw_timer;
static unsigned long sw_ticks;
static bool sw_timer_scheduled;

static void
sw_timer_run(void *arg)
{
    (void)arg;

    mutex_lock(&sw_mutex);

    if (!sw_timer_scheduled) {
        goto out;
    }

    sw_ticks++;

    if ((sw_ticks % (THREAD_SCHED_FREQ * SW_DISPLAY_INTERVAL)) == 0) {
        printf("%lu\n", sw_ticks);
    }

    /* TODO Discuss drift */
    timer_schedule(&sw_timer, timer_get_time(&sw_timer) + 1);

out:
    mutex_unlock(&sw_mutex);
}

static void
sw_start(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    mutex_lock(&sw_mutex);

    if (sw_timer_scheduled) {
        goto out;
    }

    sw_ticks = 0;
    sw_timer_scheduled = true;

    /* TODO Discuss resolution */
    timer_schedule(&sw_timer, timer_now() + 1);

out:
    mutex_unlock(&sw_mutex);
}

static void
sw_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    mutex_lock(&sw_mutex);
    sw_timer_scheduled = false;
    mutex_unlock(&sw_mutex);
}

static void
sw_resume(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    mutex_lock(&sw_mutex);
    sw_timer_scheduled = true;
    timer_schedule(&sw_timer, timer_now() + 1);
    mutex_unlock(&sw_mutex);
}

static void
sw_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    mutex_lock(&sw_mutex);
    printf("%lu\n", sw_ticks);
    mutex_unlock(&sw_mutex);
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

void
sw_setup(void)
{
    int error;

    mutex_init(&sw_mutex);
    timer_init(&sw_timer, sw_timer_run, NULL);
    sw_timer_scheduled = false;

    for (size_t i = 0; i < ARRAY_SIZE(shell_cmds); i++) {
        error = shell_cmd_register(&shell_cmds[i]);

        if (error) {
            panic("unable to register shell command");
        }
    }
}
