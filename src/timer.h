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
 * Software timer module.
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <stdbool.h>

#include <lib/list.h>

struct timer;

/*
 * Type for timer functions.
 */
typedef void (*timer_fn_t)(void *arg);

struct timer {
    struct list node;
    unsigned long ticks;
    timer_fn_t fn;
    void *arg;
};

bool timer_ticks_expired(unsigned long ticks, unsigned long ref);
bool timer_ticks_occurred(unsigned long ticks, unsigned long ref);

/*
 * Initialize the timer module.
 */
void timer_setup(void);

unsigned long timer_now(void);

void timer_init(struct timer *timer, timer_fn_t fn, void *arg);

unsigned long timer_get_time(const struct timer *timer);

void timer_schedule(struct timer *timer, unsigned long ticks);

void timer_report_tick(void);

#endif /* _TIMER_H */
