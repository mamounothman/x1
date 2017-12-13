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

#include <lib/list.h>

#include "condvar.h"
#include "mutex.h"
#include "thread.h"

struct condvar_waiter {
    struct list node;
    struct thread *thread;
    bool awaken;
};

static void
condvar_waiter_init(struct condvar_waiter *waiter, struct thread *thread)
{
    waiter->thread = thread;
    waiter->awaken = false;
}

static bool
condvar_waiter_awaken(const struct condvar_waiter *waiter)
{
    return waiter->awaken;
}

static void
condvar_waiter_wakeup(struct condvar_waiter *waiter)
{
    if (condvar_waiter_awaken(waiter)) {
        return;
    }

    thread_wakeup(waiter->thread);
    waiter->awaken = true;
}

void
condvar_init(struct condvar *condvar)
{
    list_init(&condvar->waiters);
}

void
condvar_signal(struct condvar *condvar)
{
    struct condvar_waiter *waiter;
    struct list *first;

    thread_preempt_disable();

    if (!list_empty(&condvar->waiters)) {
        first = list_first(&condvar->waiters);
        waiter = list_entry(first, struct condvar_waiter, node);
        condvar_waiter_wakeup(waiter);
    }

    thread_preempt_enable();
}

void
condvar_broadcast(struct condvar *condvar)
{
    struct condvar_waiter *waiter;

    thread_preempt_disable();

    list_for_each_entry(&condvar->waiters, waiter, node) {
        condvar_waiter_wakeup(waiter);
    }

    thread_preempt_enable();
}

void
condvar_wait(struct condvar *condvar, struct mutex *mutex)
{
    struct condvar_waiter waiter;
    struct thread *thread;

    thread = thread_self();
    condvar_waiter_init(&waiter, thread);

    thread_preempt_disable();

    mutex_unlock(mutex);

    list_insert_tail(&condvar->waiters, &waiter.node);

    do {
        thread_sleep();
    } while (!condvar_waiter_awaken(&waiter));

    list_remove(&waiter.node);

    thread_preempt_enable();

    /* TODO Discuss ordering */
    mutex_lock(mutex);
}
