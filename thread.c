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
#include <stddef.h>
#include <stdint.h>

#include <assert.h>
#include <cpu.h>
#include <error.h>
#include <fmt.h>
#include <list.h>
#include <mem.h>
#include <panic.h>
#include <thread.h>

#define THREAD_STACK_MIN_SIZE 512

struct thread_runq {
    struct thread *cur_thread;
    struct list threads;
    struct thread *idle_thread;
};

struct thread {
    struct list node;
    char name[THREAD_NAME_MAX_SIZE];
    void *stack;
};

static struct thread_runq thread_runq;

void thread_load(void);
void thread_main(thread_fn_t fn, void *arg);

static void
thread_idle(void *arg)
{
    (void)arg;

    for (;;) {
        cpu_idle();
    }
}

static void
thread_runq_init(struct thread_runq *runq)
{
    int error;

    runq->cur_thread = NULL;
    list_init(&runq->threads);

    /* TODO: Don't add idle thread to thread list */
    error = thread_create(&runq->idle_thread, thread_idle, NULL,
                          "idle", THREAD_STACK_MIN_SIZE);

    if (error) {
        panic("thread: error: unable to create idle thread");
    }
}

static struct thread *
thread_runq_get_current(struct thread_runq *runq)
{
    return runq->cur_thread;
}

void
thread_setup(void)
{
    thread_runq_init(&thread_runq);
}

void
thread_enable_scheduler(void)
{
    cpu_halt();
}

void
thread_main(thread_fn_t fn, void *arg)
{
    assert(fn);

    fn(arg);
}

const char *
thread_name(const struct thread *thread)
{
    return thread->name;
}

static void
thread_set_name(struct thread *thread, const char *name)
{
    fmt_snprintf(thread->name, sizeof(thread->name), "%s", name);
}

static void
thread_stack_push(uint32_t **stackp, uint32_t word)
{
    uint32_t *stack;

    stack = *stackp;
    stack--;
    *stack = word;
    *stackp = stack;
}

static void
thread_stack_forge(char *stack_addr, size_t stack_size,
                   thread_fn_t fn, void *arg)
{
    uint32_t *stack;

    stack = (uint32_t *)(stack_addr + stack_size);
    thread_stack_push(&stack, (uint32_t)arg);
    thread_stack_push(&stack, (uint32_t)fn);
}

int
thread_create(struct thread **threadp, thread_fn_t fn, void *arg,
              const char *name, size_t stack_size)
{
    struct thread *thread;

    assert(fn);

    /* TODO Check stack size & alignment */

    thread = mem_alloc(sizeof(*thread));

    if (thread == NULL) {
        return ERROR_NOMEM;
    }

    thread->stack = mem_alloc(stack_size);

    if (thread->stack == NULL) {
        mem_free(thread);
        return ERROR_NOMEM;
    }

    thread_stack_forge(thread->stack, stack_size, fn, arg);
    thread_set_name(thread, name);

    /* TODO Method */
    list_insert_head(&thread_runq.threads, &thread->node);

    *threadp = thread;
    return 0;
}

struct thread *
thread_self(void)
{
    return thread_runq_get_current(&thread_runq);
}
