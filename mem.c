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

#include <list.h>
#include <mem.h>

#define MEM_HEAP_SIZE       (32 * 1024 * 1024)

#define MEM_ALIGN           4

#define MEM_BLOCK_MIN_SIZE  P2ROUND(((sizeof(struct mem_btag) * 2) \
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

static void *
mem_heap_end(void)
{
    return &mem_heap[sizeof(mem_heap)];
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

static struct mem_block *
mem_block_from_payload(void *payload)
{
    size_t offset;

    offset = offsetof(struct mem_block, payload);
    return (struct mem_block *)((char *)payload - offset);
}

static void *
mem_block_end(const struct mem_block *block)
{
    struct mem_block *next;

#if 1
    next = (struct mem_block *)((char *)block + mem_block_size(block));
#else
    next = (void *)block + mem_block_size(block);
#endif

    return next;
}

static struct mem_btag *
mem_block_header_btag(struct mem_block *block)
{
    return &block->btag;
}

static struct mem_btag *
mem_block_footer_btag(struct mem_block *block)
{
    struct mem_btag *btag;

    btag = mem_block_end(block);
    return &btag[-1];
}

static struct mem_block *
mem_block_prev(struct mem_block *block)
{
    struct mem_btag *btag;

    if ((char *)block == mem_heap) {
        return NULL;
    }

    btag = mem_block_header_btag(block);
    return (struct mem_block *)((char *)block - btag[-1].size);
}

static struct mem_block *
mem_block_next(struct mem_block *block)
{
    block = mem_block_end(block);

    if ((void *)block == mem_heap_end()) {
        return NULL;
    }

    return block;
}

static bool
mem_block_allocated(struct mem_block *block)
{
    return mem_btag_allocated(mem_block_header_btag(block));
}

static void
mem_block_set_allocated(struct mem_block *block)
{
    mem_btag_set_allocated(mem_block_header_btag(block));
    mem_btag_set_allocated(mem_block_footer_btag(block));
}

static void
mem_block_clear_allocated(struct mem_block *block)
{
    mem_btag_clear_allocated(mem_block_header_btag(block));
    mem_btag_clear_allocated(mem_block_footer_btag(block));
}

static void
mem_block_init(struct mem_block *block, size_t size)
{
    mem_btag_init(mem_block_header_btag(block), size);
    mem_btag_init(mem_block_footer_btag(block), size);
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

static bool
mem_block_inside_heap(const struct mem_block *block)
{
    void *heap_end;

    heap_end = mem_heap_end();
    return (((char *)block >= mem_heap)
            && ((void *)block->payload < heap_end)
            && ((void *)mem_block_end(block) <= heap_end));
}

static void
mem_flist_add(struct mem_flist *list, struct mem_block *block)
{
    struct mem_fheader *header;

    assert(mem_block_allocated(block));

    mem_block_clear_allocated(block);
    header = mem_block_header(block);
    /* TODO Explain cache-hot data */
    list_insert_head(&list->headers, &header->node);
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
        block = mem_block_from_payload(header);

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

static bool
mem_block_inside(struct mem_block *block, void *addr)
{
    return (addr >= (void *)block) && (addr < mem_block_end(block));
}

static bool
mem_block_overlap(struct mem_block *block1, struct mem_block *block2)
{
    return mem_block_inside(block1, block2)
           || mem_block_inside(block2, block1);
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
    block2 = mem_block_end(block);
    mem_block_init(block2, total_size - size);

    return block2;
}

static void
mem_block_merge(struct mem_block *block1, struct mem_block *block2)
{
    size_t size;

    assert(!mem_block_overlap(block1, block2));

    if (mem_block_allocated(block1) || mem_block_allocated(block2)) {
        return;
    }

    mem_flist_remove(&mem_flist, block1);
    mem_flist_remove(&mem_flist, block2);

    if (block1 > block2) {
        block1 = block2;
    }

    size = mem_block_size(block1) + mem_block_size(block2);
    mem_block_init(block1, size);
    mem_flist_add(&mem_flist, block1);
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
    size += sizeof(struct mem_btag) * 2;

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

    return mem_block_payload(block);
}

void
mem_free(void *ptr)
{
    struct mem_block *block, *tmp;

    block = mem_block_from_payload(ptr);
    assert(mem_block_inside_heap(block));

    mem_flist_add(&mem_flist, block);

    tmp = mem_block_prev(block);

    if (tmp) {
        mem_block_merge(block, tmp);
        block = tmp;
    }

    tmp = mem_block_next(block);

    if (tmp) {
        mem_block_merge(block, tmp);
    }
}
