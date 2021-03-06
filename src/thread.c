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
#include <stdio.h>
#include <stdlib.h>

#include <lib/macros.h>
#include <lib/list.h>

#include "cpu.h"
#include "error.h"
#include "panic.h"
#include "thread.h"
#include "timer.h"

struct thread_list {
    struct list threads;
};

struct thread_runq {
    struct thread *current;
    unsigned int nr_threads;
    struct thread_list lists[THREAD_NR_PRIORITIES];
    struct thread *idle;
};

enum thread_state {
    THREAD_STATE_RUNNING,
    THREAD_STATE_SLEEPING,
    THREAD_STATE_DEAD,
};

struct thread {
    void *sp;
    enum thread_state state;
    bool yield;
    struct list node;
    unsigned int preempt_level;
    unsigned int priority;
    struct thread *joiner;
    char name[THREAD_NAME_MAX_SIZE];
    void *stack;
};

static struct thread_runq thread_runq;

static struct thread thread_dummy;

void thread_load_context(struct thread *thread) __attribute__((noreturn));
void thread_switch_context(struct thread *prev, struct thread *next);
void thread_start(void);
void thread_main(thread_fn_t fn, void *arg);

static void
thread_idle(void *arg)
{
    (void)arg;

    for (;;) {
        cpu_idle();
    }
}

static bool
thread_is_running(const struct thread *thread)
{
    return thread->state == THREAD_STATE_RUNNING;
}

static void
thread_set_running(struct thread *thread)
{
    thread->state = THREAD_STATE_RUNNING;
}

static void
thread_set_sleeping(struct thread *thread)
{
    thread->state = THREAD_STATE_SLEEPING;
}

static bool
thread_is_dead(const struct thread *thread)
{
    return thread->state == THREAD_STATE_DEAD;
}

static void
thread_set_dead(struct thread *thread)
{
    thread->state = THREAD_STATE_DEAD;
}

static bool
thread_should_yield(const struct thread *thread)
{
    return thread->yield;
}

static void
thread_set_yield(struct thread *thread)
{
    thread->yield = true;
}

static void
thread_clear_yield(struct thread *thread)
{
    thread->yield = false;
}

static unsigned int
thread_get_priority(struct thread *thread)
{
    return thread->priority;
}

static void
thread_remove_from_list(struct thread *thread)
{
    list_remove(&thread->node);
}

static bool
thread_scheduler_locked(void)
{
    return !cpu_intr_enabled() && !thread_preempt_enabled();
}

static uint32_t
thread_lock_scheduler(void)
{
    /* TODO Explain order */
    thread_preempt_disable();
    return cpu_intr_save();
}

static void
thread_unlock_scheduler(uint32_t eflags, bool yield)
{
    cpu_intr_restore(eflags);

    if (yield) {
        thread_preempt_enable();
    } else {
        thread_preempt_enable_no_yield();
    }
}

static void
thread_list_init(struct thread_list *list)
{
    list_init(&list->threads);
}

static void
thread_list_enqueue(struct thread_list *list, struct thread *thread)
{
    list_insert_tail(&list->threads, &thread->node);
}

static struct thread *
thread_list_dequeue(struct thread_list *list)
{
    struct thread *thread;

    thread = list_first_entry(&list->threads, typeof(*thread), node);
    thread_remove_from_list(thread);
    return thread;
}

static bool
thread_list_empty(struct thread_list *list)
{
    return list_empty(&list->threads);
}

static struct thread_list *
thread_runq_get_list(struct thread_runq *runq, unsigned int priority)
{
    assert(priority < ARRAY_SIZE(runq->lists));
    return &runq->lists[priority];
}

static struct thread *
thread_runq_get_current(struct thread_runq *runq)
{
    return runq->current;
}

static void
thread_runq_put_prev(struct thread_runq *runq, struct thread *thread)
{
    struct thread_list *list;

    if (thread == runq->idle) {
        return;
    }

    list = thread_runq_get_list(runq, thread_get_priority(thread));
    thread_list_enqueue(list, thread);
}

static struct thread *
thread_runq_get_next(struct thread_runq *runq)
{
    struct thread *thread;

    assert(runq->current);

    if (runq->nr_threads == 0) {
        thread = runq->idle;
    } else {
        struct thread_list *list;
        size_t nr_lists;

        nr_lists = ARRAY_SIZE(runq->lists);

        /* TODO Explain condition */
        for (size_t i = (nr_lists - 1); i < nr_lists; i--) {
            list = thread_runq_get_list(runq, i);

            if (!thread_list_empty(list)) {
                break;
            }
        }

        thread = thread_list_dequeue(list);
    }

    runq->current = thread;
    return thread;
}

static void
thread_runq_add(struct thread_runq *runq, struct thread *thread)
{
    struct thread_list *list;

    assert(thread_scheduler_locked());
    assert(thread_is_running(thread));

    list = thread_runq_get_list(runq, thread_get_priority(thread));
    thread_list_enqueue(list, thread);

    runq->nr_threads++;
    assert(runq->nr_threads != 0);

    if (thread_get_priority(thread) > thread_get_priority(runq->current)) {
        thread_set_yield(runq->current);
    }
}

static void
thread_runq_remove(struct thread_runq *runq, struct thread *thread)
{
    assert(runq->nr_threads != 0);
    runq->nr_threads--;

    assert(!thread_is_running(thread));
    thread_remove_from_list(thread);
}

static void
thread_runq_schedule(struct thread_runq *runq)
{
    struct thread *prev, *next;

    prev = thread_runq_get_current(runq);

    assert(thread_scheduler_locked());
    assert(prev->preempt_level == 1);

    thread_runq_put_prev(runq, prev);

    if (!thread_is_running(prev)) {
        thread_runq_remove(runq, prev);
    }

    next = thread_runq_get_next(runq);

    if (prev != next) {
        /* TODO Explain how this acts as a compiler barrier */
        thread_switch_context(prev, next);
    }
}

void
thread_enable_scheduler(void)
{
    struct thread *thread;

    thread = thread_runq_get_next(&thread_runq);
    assert(thread);
    assert(thread->preempt_level == 1);
    thread_load_context(thread);

    /* Never reached */
}

void
thread_main(thread_fn_t fn, void *arg)
{
    assert(fn);

    assert(thread_scheduler_locked());
    assert(thread_self()->preempt_level == 1);

    cpu_intr_enable();
    thread_preempt_enable();

    fn(arg);

    thread_exit();
}

const char *
thread_name(const struct thread *thread)
{
    return thread->name;
}

static void
thread_set_name(struct thread *thread, const char *name)
{
    snprintf(thread->name, sizeof(thread->name), "%s", name);
}

static void
thread_stack_push(uint32_t **stackp, size_t *stack_sizep, uint32_t word)
{
    uint32_t *stack;
    size_t stack_size;

    stack = *stackp;
    stack_size = *stack_sizep;
    assert(stack_size >= sizeof(word));
    stack--;
    stack_size -= sizeof(word);
    *stack = word;
    *stackp = stack;
    *stack_sizep = stack_size;
}

static void *
thread_stack_forge(char *stack_addr, size_t stack_size,
                   thread_fn_t fn, void *arg)
{
    uint32_t *stack;

    stack = (uint32_t *)(stack_addr + stack_size);
    thread_stack_push(&stack, &stack_size, (uint32_t)arg);
    thread_stack_push(&stack, &stack_size, (uint32_t)fn);
    thread_stack_push(&stack, &stack_size, (uint32_t)thread_start);
    thread_stack_push(&stack, &stack_size, 0); /* EBP */
    thread_stack_push(&stack, &stack_size, 0); /* EBX */
    thread_stack_push(&stack, &stack_size, 0); /* EDI */
    thread_stack_push(&stack, &stack_size, 0); /* ESI */
    thread_stack_push(&stack, &stack_size, CPU_EFL_ONE); /* EFLAGS */
    return stack;
}

static void
thread_init(struct thread *thread, thread_fn_t fn, void *arg,
            const char *name, char *stack, size_t stack_size,
            unsigned int priority)
{
    /* TODO Describe the preempted state */

    if (stack) {
        thread->sp = thread_stack_forge(stack, stack_size, fn, arg);
    }

    thread->state = THREAD_STATE_RUNNING;
    thread->yield = false;
    thread->preempt_level = 1;
    thread->priority = priority;
    thread->joiner = NULL;
    thread_set_name(thread, name);
    thread->stack = stack;
}

int
thread_create(struct thread **threadp, thread_fn_t fn, void *arg,
              const char *name, size_t stack_size, unsigned int priority)
{
    struct thread *thread;
    uint32_t eflags;
    void *stack;

    assert(fn);

    /* TODO Check stack size & alignment */

    thread = malloc(sizeof(*thread));

    if (!thread) {
        return ERROR_NOMEM;
    }

    stack = malloc(stack_size);

    if (!stack) {
        free(thread);
        return ERROR_NOMEM;
    }

    thread_init(thread, fn, arg, name, stack, stack_size, priority);

    eflags = thread_lock_scheduler();
    thread_runq_add(&thread_runq, thread);
    thread_unlock_scheduler(eflags, true);

    if (threadp) {
        *threadp = thread;
    }

    return 0;
}

static void
thread_destroy(struct thread *thread)
{
    assert(thread_is_dead(thread));

    free(thread->stack);
    free(thread);
}

void
thread_exit(void)
{
    struct thread *thread;

    thread = thread_self();

    assert(thread_preempt_enabled());

    thread_lock_scheduler();
    assert(thread_is_running(thread));
    thread_set_dead(thread);
    thread_wakeup(thread->joiner);
    thread_runq_schedule(&thread_runq);

    panic("thread: error: dead thread walking");
}

void
thread_join(struct thread *thread)
{
    uint32_t eflags;

    eflags = thread_lock_scheduler();

    thread->joiner = thread_self();

    while (!thread_is_dead(thread)) {
        thread_sleep();
    }

    thread_unlock_scheduler(eflags, true);

    thread_destroy(thread);
}

struct thread *
thread_self(void)
{
    return thread_runq_get_current(&thread_runq);
}

static struct thread *
thread_create_idle(void)
{
    struct thread *idle;
    void *stack;

    idle = malloc(sizeof(*idle));

    if (!idle) {
        panic("thread: unable to allocate idle thread");
    }

    stack = malloc(THREAD_STACK_MIN_SIZE);

    if (!stack) {
        panic("thread: unable to allocate idle thread stack");
    }

    thread_init(idle, thread_idle, NULL, "idle",
                stack, THREAD_STACK_MIN_SIZE, THREAD_IDLE_PRIORITY);
    return idle;
}

static void
thread_runq_preinit(struct thread_runq *runq)
{
    thread_init(&thread_dummy, NULL, NULL, "dummy", NULL, 0, 0);
    runq->current = &thread_dummy;
}

static void
thread_runq_init(struct thread_runq *runq)
{
    runq->nr_threads = 0;

    for (size_t i = 0; i < ARRAY_SIZE(runq->lists); i++) {
        thread_list_init(&runq->lists[i]);
    }

    runq->idle = thread_create_idle();
}

void
thread_bootstrap(void)
{
    thread_runq_preinit(&thread_runq);
}

void
thread_setup(void)
{
    thread_runq_init(&thread_runq);
}

void
thread_yield(void)
{
    uint32_t eflags;

    if (!thread_preempt_enabled()) {
        return;
    }

    eflags = thread_lock_scheduler();
    thread_clear_yield(thread_self());
    thread_runq_schedule(&thread_runq);
    thread_unlock_scheduler(eflags, false);
}

void
thread_yield_if_needed(void)
{
    if (thread_should_yield(thread_self())) {
        thread_yield();
    }
}

void
thread_sleep(void)
{
    struct thread *thread;
    uint32_t eflags;

    thread = thread_self();

    eflags = cpu_intr_save();
    assert(thread_is_running(thread));
    thread_set_sleeping(thread);
    thread_runq_schedule(&thread_runq);
    assert(thread_is_running(thread));
    cpu_intr_restore(eflags);
}

void
thread_wakeup(struct thread *thread)
{
    uint32_t eflags;

    if (!thread || (thread == thread_self())) {
        return;
    }

    eflags = thread_lock_scheduler();

    if (!thread_is_running(thread)) {
        assert(!thread_is_dead(thread));
        thread_set_running(thread);
        thread_runq_add(&thread_runq, thread);
    }

    thread_unlock_scheduler(eflags, true);
}

void
thread_preempt_disable(void)
{
    struct thread *thread;

    thread = thread_self();
    thread->preempt_level++;
    assert(thread->preempt_level != 0);

    barrier();
}

void
thread_preempt_enable_no_yield(void)
{
    struct thread *thread;

    barrier();

    thread = thread_self();
    assert(thread->preempt_level != 0);
    thread->preempt_level--;
}

void
thread_preempt_enable(void)
{
    thread_preempt_enable_no_yield();
    thread_yield_if_needed();
}

bool
thread_preempt_enabled(void)
{
    struct thread *thread;

    thread = thread_self();
    return thread->preempt_level == 0;
}

void
thread_report_tick(void)
{
    thread_set_yield(thread_self());
    timer_report_tick();
}
