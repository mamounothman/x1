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

#include <lib/list.h>
#include <lib/macros.h>

#include "cpu.h"
#include "mutex.h"
#include "panic.h"
#include "thread.h"
#include "timer.h"

#define TIMER_STACK_SIZE 4096

#define TIMER_THRESHOLD (((unsigned long)-1) / 2)

static unsigned long timer_ticks;

static bool timer_list_empty;
static unsigned long timer_wakeup_ticks;

static struct list timer_list;
static struct mutex timer_mutex;

static struct thread *timer_thread;

static bool
timer_ticks_expired(unsigned long ticks, unsigned long ref)
{
    return (ticks - ref) > TIMER_THRESHOLD;
}

static bool
timer_ticks_occurred(unsigned long ticks, unsigned long ref)
{
    return (ticks == ref) || timer_ticks_expired(ticks, ref);
}

static bool
timer_work_pending(void)
{
    assert(!cpu_intr_enabled());

    return !timer_list_empty
           && timer_ticks_occurred(timer_wakeup_ticks, timer_ticks);
}

static bool
timer_expired(const struct timer *timer, unsigned long ref)
{
    return timer_ticks_expired(timer->ticks, ref);
}

static bool
timer_occurred(const struct timer *timer, unsigned long ref)
{
    return timer_ticks_occurred(timer->ticks, ref);
}

static void
timer_process(struct timer *timer)
{
    timer->fn(timer->arg);
}

static void
timer_process_list(unsigned long now)
{
    struct timer *timer;
    uint32_t eflags;

    mutex_lock(&timer_mutex);

    while (!list_empty(&timer_list)) {
        timer = list_first_entry(&timer_list, typeof(*timer), node);

        if (!timer_occurred(timer, now)) {
            break;
        }

        list_remove(&timer->node);
        mutex_unlock(&timer_mutex);

        timer_process(timer);

        mutex_lock(&timer_mutex);
    }

    eflags = cpu_intr_save();

    timer_list_empty = list_empty(&timer_list);

    if (!timer_list_empty) {
        timer = list_first_entry(&timer_list, typeof(*timer), node);
        timer_wakeup_ticks = timer->ticks;
    }

    cpu_intr_restore(eflags);

    mutex_unlock(&timer_mutex);
}

static void
timer_run(void *arg)
{
    unsigned long now;
    uint32_t eflags;

    (void)arg;

    for (;;) {
        thread_preempt_disable();
        eflags = cpu_intr_save();

        for (;;) {
            now = timer_ticks;

            if (timer_work_pending()) {
                break;
            }

            thread_sleep();
        }

        cpu_intr_restore(eflags);
        thread_preempt_enable();

        timer_process_list(now);
    }
}

void
timer_setup(void)
{
    int error;

    timer_ticks = 0;
    timer_list_empty = true;

    list_init(&timer_list);
    mutex_init(&timer_mutex);

    error = thread_create(&timer_thread, timer_run, NULL,
                          "timer", TIMER_STACK_SIZE);

    if (error) {
        panic("timer: unable to create thread");
    }
}

unsigned long
timer_now(void)
{
    unsigned long ticks;
    uint32_t eflags;

    eflags = cpu_intr_save();
    ticks = timer_ticks;
    cpu_intr_restore(eflags);

    return ticks;
}

void
timer_init(struct timer *timer, timer_fn_t fn, void *arg)
{
    timer->fn = fn;
    timer->arg = arg;
}

unsigned long
timer_get_time(const struct timer *timer)
{
    unsigned long ticks;

    mutex_lock(&timer_mutex);
    ticks = timer->ticks;
    mutex_unlock(&timer_mutex);

    return ticks;
}

void
timer_schedule(struct timer *timer, unsigned long ticks)
{
    struct timer *tmp;
    uint32_t eflags;

    mutex_lock(&timer_mutex);

    timer->ticks = ticks;

    list_for_each_entry(&timer_list, tmp, node) {
        if (!timer_expired(tmp, ticks)) {
            break;
        }
    }

    list_insert_before(&tmp->node, &timer->node);

    timer = list_first_entry(&timer_list, typeof(*timer), node);

    eflags = cpu_intr_save();
    timer_list_empty = false;
    timer_wakeup_ticks = timer->ticks;
    cpu_intr_restore(eflags);

    /* TODO Explain how unlocking here avoids a spurious wake-up */
    mutex_unlock(&timer_mutex);
}

void
timer_report_tick(void)
{
    timer_ticks++;

    if (timer_work_pending()) {
        thread_wakeup(timer_thread);
    }
}
