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

#include <list.h>
#include <mem.h>

#define MEM_HEAP_SIZE       (32 * 1024 * 1024)

#define MEM_ALIGN           4

#define MEM_BLOCK_MIN_SIZE  P2ROUND((sizeof(struct mem_btag) \
                                    + sizeof(struct mem_fheader)), MEM_ALIGN)

struct mem_fheader {
    struct list node;
};

struct mem_btag {
    size_t size;
    bool allocated;
};

struct mem_block {
    struct mem_btag btag;
    char payload[];
};

struct mem_flist {
    struct list headers;
};

static char mem_heap[MEM_HEAP_SIZE] __aligned(MEM_ALIGN);
static struct mem_flist mem_flist;

static struct mem_block *
mem_fheader_get_block(struct mem_fheader *header)
{
    size_t offset;

    offset = offsetof(struct mem_block, payload);
    return (struct mem_block *)((char *)header - offset);
}

static void
mem_btag_init(struct mem_btag *btag, size_t size)
{
    btag->size = size;
    btag->allocated = true;
}

static bool
mem_btag_allocated(const struct mem_btag *btag)
{
    return btag->allocated;
}

static void
mem_btag_set_allocated(struct mem_btag *btag)
{
    btag->allocated = true;
}

static void
mem_btag_clear_allocated(struct mem_btag *btag)
{
    btag->allocated = false;
}

static size_t
mem_btag_size(const struct mem_btag *btag)
{
    return btag->size;
}

static size_t
mem_block_size(const struct mem_block *block)
{
    return mem_btag_size(&block->btag);
}

static bool
mem_block_allocated(const struct mem_block *block)
{
    return mem_btag_allocated(&block->btag);
}

static void
mem_block_set_allocated(struct mem_block *block)
{
    mem_btag_set_allocated(&block->btag);
}

static void
mem_block_clear_allocated(struct mem_block *block)
{
    mem_btag_clear_allocated(&block->btag);
}

static struct mem_block *
mem_block_get_next(struct mem_block *block)
{
    struct mem_block *next;

#if 1
    next = (struct mem_block *)((char *)block + mem_block_size(block));
#else
    next = (void *)block + mem_block_size(block);
#endif

    return next;
}

static void
mem_block_init(struct mem_block *block, size_t size)
{
    mem_btag_init(&block->btag, size);
}

static void *
mem_block_payload(struct mem_block *block)
{
    return block->payload;
}

static struct mem_fheader *
mem_block_header(struct mem_block *block)
{
    assert(!mem_block_allocated(block));
    return mem_block_payload(block);
}

static struct mem_block *
mem_block_split(struct mem_block *block, size_t size)
{
    struct mem_block *block2;
    size_t total_size;

    assert(mem_block_allocated(block));
    assert(P2ALIGNED(size, MEM_ALIGN));

    if (mem_block_size(block) < (size + MEM_BLOCK_MIN_SIZE)) {
        return NULL;
    }

    total_size = mem_block_size(block);
    mem_block_init(block, size);
    block2 = mem_block_get_next(block);
    mem_block_init(block2, total_size - size);

    return block2;
}

static void
mem_flist_add(struct mem_flist *list, struct mem_block *block)
{
    struct mem_fheader *header;

    assert(mem_block_allocated(block));

    mem_block_clear_allocated(block);
    header = mem_block_header(block);
    list_insert_tail(&list->headers, &header->node);
}

static void
mem_flist_remove(struct mem_flist *list, struct mem_block *block)
{
    struct mem_fheader *header;

    (void)list;

    assert(!mem_block_allocated(block));

    header = mem_block_header(block);
    list_remove(&header->node);
    mem_block_set_allocated(block);
}

static struct mem_block *
mem_flist_find(struct mem_flist *list, size_t size)
{
    struct mem_fheader *header;
    struct mem_block *block;

    list_for_each_entry(&list->headers, header, node) {
        block = mem_fheader_get_block(header);

        if (mem_block_size(block) >= size) {
            return block;
        }
    }

    return NULL;
}

static void
mem_flist_init(struct mem_flist *list)
{
    list_init(&list->headers);
}

void
mem_setup(void)
{
    struct mem_block *block;

    block = (struct mem_block *)mem_heap;
    mem_block_init(block, sizeof(mem_heap));
    mem_flist_init(&mem_flist);
    mem_flist_add(&mem_flist, block);
}

static size_t
mem_convert_to_block_size(size_t size)
{
    size = P2ROUND(size, MEM_ALIGN);
    size += sizeof(struct mem_btag);

    if (size < MEM_BLOCK_MIN_SIZE) {
        size = MEM_BLOCK_MIN_SIZE;
    }

    return size;
}

void *
mem_alloc(size_t size)
{
    struct mem_block *block, *block2;

    if (size == 0) {
        return NULL;
    }

    size = mem_convert_to_block_size(size);
    block = mem_flist_find(&mem_flist, size);

    if (block == NULL) {
        return NULL;
    }

    mem_flist_remove(&mem_flist, block);
    block2 = mem_block_split(block, size);

    if (block2 != NULL) {
        mem_flist_add(&mem_flist, block2);
    }

    return block;
}

void
mem_free(void *ptr)
{
    (void)ptr;
}
