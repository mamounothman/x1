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

#include <list.h>
#include <mem.h>

#define MEM_HEAP_SIZE (32*1024*1024)

struct mem_fheader {
    struct list node;
};

struct mem_btag {
    size_t size;
    bool free;
};

struct mem_hbtag {
    struct mem_btag btag;
    struct mem_fheader header[];
};

struct mem_fbtag {
    struct mem_btag btag;
    struct mem_hbtag hbtag[];
};

struct mem_block {
    struct mem_hbtag hbtag;
};

struct mem_flist {
    struct list headers;
};

static char mem_heap[MEM_HEAP_SIZE] __aligned(4);
static struct mem_flist mem_flist;

static void
mem_btag_init(struct mem_btag *btag, size_t size)
{
    btag->size = size;
    btag->free = true;
}

static void
mem_hbtag_init(struct mem_hbtag *hbtag, size_t size)
{
    mem_btag_init(&hbtag->btag, size);
}

static void
mem_fbtag_init(struct mem_fbtag *fbtag, size_t size)
{
    mem_btag_init(&fbtag->btag, size);
}

static size_t
mem_block_size(const struct mem_block *block)
{
    return block->hbtag.btag.size; /* TODO Refactor */
}

static struct mem_fbtag *
mem_block_get_fbtag(struct mem_block *block)
{
    struct mem_fbtag *fbtag;

#if 1
    fbtag = (struct mem_fbtag *)((char *)block + mem_block_size(block));
#else
    fbtag = (void *)block + mem_block_size(block);
#endif

    return fbtag - 1;
}

static void
mem_block_init(struct mem_block *block, size_t size)
{
    mem_hbtag_init(&block->hbtag, size);
    mem_fbtag_init(mem_block_get_fbtag(block), size);
}

static struct mem_fheader *
mem_block_header(struct mem_block *block)
{
    return block->hbtag.header;
}

static void
mem_flist_add_header(struct mem_flist *list, struct mem_fheader *header)
{
    list_insert_tail(&list->headers, &header->node);
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
    mem_flist_add_header(&mem_flist, mem_block_header(block));
}

void *
mem_alloc(size_t size)
{
    return NULL;
}

void
mem_free(void *ptr)
{

}

