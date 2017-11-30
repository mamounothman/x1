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

#include <cpu.h>
#include <error.h>
#include <fmt.h>
#include <list.h>
#include <mem.h>
#include <panic.h>
#include <thread.h>

#define THREAD_STACK_MIN_SIZE 512

struct thread_runq {
    struct thread *current;
    struct list threads;
    struct thread *idle;
};

struct thread {
    void *sp;
    struct list node;
    unsigned int preempt_level;
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

static struct thread *
thread_runq_get_current(struct thread_runq *runq)
{
    return runq->current;
}

static struct thread *
thread_runq_get_next(struct thread_runq *runq)
{
    struct thread *thread;
    struct list *node;

    assert(runq->current);

    if (list_empty(&runq->threads)) {
        thread = runq->idle;
    } else {
        node = list_next(&runq->current->node);

        if (list_end(&runq->threads, node)) {
            node = list_first(&runq->threads);
        }

        thread = list_entry(node, struct thread, node);
    }

    runq->current = thread;
    return thread;
}

static void
thread_runq_add(struct thread_runq *runq, struct thread *thread)
{
    list_insert_head(&runq->threads, &thread->node);
}

static void
thread_runq_start(struct thread_runq *runq)
{
    if (list_empty(&runq->threads)) {
        runq->current = runq->idle;
    } else {
        runq->current = list_first_entry(&runq->threads, struct thread, node);
    }
}

static void
thread_runq_schedule(struct thread_runq *runq)
{
    struct thread *prev, *next;
    uint32_t eflags;

    eflags = cpu_intr_save();
    thread_preempt_disable();

    prev = thread_runq_get_current(runq);
    next = thread_runq_get_next(runq);

    if (prev != next) {
        thread_switch_context(prev, next);
    }

    thread_preempt_enable();
    cpu_intr_restore(eflags);
}

void
thread_enable_scheduler(void)
{
    struct thread *thread;

    thread_runq_start(&thread_runq);
    thread = thread_runq_get_current(&thread_runq);
    assert(thread);
    assert(thread->preempt_level == 1);
    thread_load_context(thread);

    /* Never reached */
}

void
thread_main(thread_fn_t fn, void *arg)
{
    assert(fn);

    assert(!cpu_intr_enabled());
    assert(thread_self()->preempt_level == 1);

    fn(arg);

    /* TODO Destruction */
    cpu_halt();
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
            const char *name, char *stack, size_t stack_size)
{
    /* TODO Describe the preempted state */

    if (stack) {
        thread->sp = thread_stack_forge(stack, stack_size, fn, arg);
    }

    thread->preempt_level = 1;
    thread_set_name(thread, name);
    thread->stack = stack;
}

int
thread_create(struct thread **threadp, thread_fn_t fn, void *arg,
              const char *name, size_t stack_size)
{
    struct thread *thread;
    void *stack;

    assert(fn);

    /* TODO Check stack size & alignment */

    thread = mem_alloc(sizeof(*thread));

    if (!thread) {
        return ERROR_NOMEM;
    }

    stack = mem_alloc(stack_size);

    if (!stack) {
        mem_free(thread);
        return ERROR_NOMEM;
    }

    thread_init(thread, fn, arg, name, stack, stack_size);
    thread_runq_add(&thread_runq, thread);

    *threadp = thread;
    return 0;
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

    idle = mem_alloc(sizeof(*idle));

    if (!idle) {
        panic("thread: unable to allocate idle thread");
    }

    stack = mem_alloc(THREAD_STACK_MIN_SIZE);

    if (!stack) {
        panic("thread: unable to allocate idle thread stack");
    }

    thread_init(idle, thread_idle, NULL, "idle",
                stack, THREAD_STACK_MIN_SIZE);
    return idle;
}

static void
thread_runq_preinit(struct thread_runq *runq)
{
    thread_init(&thread_dummy, NULL, NULL, "dummy", NULL, 0);
    runq->current = &thread_dummy;
}

static void
thread_runq_init(struct thread_runq *runq)
{
    list_init(&runq->threads);
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
    thread_runq_schedule(&thread_runq);
}

void
thread_preempt_enable(void)
{
    struct thread *thread;

    barrier();

    thread = thread_self();
    assert(thread->preempt_level != 0);
    thread->preempt_level--;

    /* TODO Reschedule if necessary */
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

bool
thread_preempt_enabled(void)
{
    struct thread *thread;

    thread = thread_self();
    return thread->preempt_level == 0;
}
