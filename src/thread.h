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

#ifndef _THREAD_H
#define _THREAD_H

#include <stdbool.h>
#include <stddef.h>

#define THREAD_SCHED_FREQ 100

#define THREAD_NAME_MAX_SIZE 16

#define THREAD_STACK_MIN_SIZE 4096

#define THREAD_NR_PRIORITIES    20
#define THREAD_IDLE_PRIORITY    0
#define THREAD_MIN_PRIORITY     1
#define THREAD_MAX_PRIORITY     (THREAD_NR_PRIORITIES - 1)

typedef void (*thread_fn_t)(void *arg);

struct thread;

void thread_bootstrap(void);
void thread_setup(void);

int thread_create(struct thread **threadp, thread_fn_t fn, void *arg,
                  const char *name, size_t stack_size, unsigned int priority);
void thread_exit(void) __attribute__((noreturn));
void thread_join(struct thread *thread);

struct thread * thread_self(void);
const char * thread_name(const struct thread *thread);

void thread_yield(void);
void thread_yield_if_needed(void);

void thread_sleep(void);
void thread_wakeup(struct thread *thread);

void thread_preempt_enable_no_yield(void);
void thread_preempt_enable(void);
void thread_preempt_disable(void);
bool thread_preempt_enabled(void);

void thread_report_tick(void);

void thread_enable_scheduler(void) __attribute__((noreturn));

#endif /* _THREAD_H */
