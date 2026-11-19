
// myalloc.c - my own malloc and free built on top of sbrk
// Task 1: block header and helper functions
// Task 2: growing the heap with sbrk
// Task 3: my_malloc with first-fit
// Task 4: splitting blocks
// Task 5: my_free and coalescing
// Task 6: my_calloc and my_realloc
// Task 7: best-fit, heap statistics and heap dump


#define _DEFAULT_SOURCE

#include <stddef.h>    
#include <stdint.h>    
#include <string.h>    
#include <unistd.h>    // sbrk, write
#include <stdio.h>     // snprintf
#include "myalloc.h"   // API, fit_policy_t and heap_stats_t

#define ALIGNMENT 16
#define MAGIC     0x4D414C43
#define CHUNK     ((size_t)64 * 1024)

// messages printed by my_free and my_realloc
#define MSG_INVALID "my_free: invalid pointer\n"
#define MSG_DOUBLE  "my_free: double free detected\n"
#define MSG_REALLOC "my_realloc: invalid pointer\n"

// block header
typedef struct block {
    size_t        size;
    struct block *next;
    int           free;
    int           magic;
} block_t;

// header size rounded up to a multiple of 16
#define HDR ((sizeof(block_t) + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1))

_Static_assert(HDR % ALIGNMENT == 0, "HDR must be a multiple of 16");

// round n up to the next multiple of 16
static inline size_t align_up(size_t n)
{
    return (n + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
}

// get the payload pointer from a block header
static inline void *payload(block_t *b)
{
    return (char *)b + HDR;
}

// get the block header from a payload pointer
static inline block_t *header(void *ptr)
{
    return (block_t *)((char *)ptr - HDR);
}

// The list of blocks and growing the heap

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t heap_total = 0;

// Task 7: allocation policy (first-fit is the default)
static fit_policy_t current_policy = FIT_FIRST;

// get more memory from the OS and add it to the end of the list
static block_t *grow(size_t need)
{
    size_t   total;
    size_t   pad;
    void    *cur;
    void    *old;
    block_t *b;

    if (need > (size_t)INTPTR_MAX / 2) {
        return NULL;
    }

    total = HDR + need;

    if (total < CHUNK) {
        total = CHUNK;
    }

    total = align_up(total);

    cur = sbrk(0);

    if (cur == (void *)-1) {
        return NULL;
    }

    pad = (ALIGNMENT - ((uintptr_t)cur % ALIGNMENT)) % ALIGNMENT;

    old = sbrk((intptr_t)(pad + total));

    if (old == (void *)-1) {
        return NULL;
    }

    b = (block_t *)((char *)old + pad);

    b->size  = total - HDR;
    b->next  = NULL;
    b->free  = 1;
    b->magic = MAGIC;

    if (tail == NULL) {
        head = b;
    } else {
        tail->next = b;
    }

    tail = b;

    heap_total += pad + total;

    return b;
}

// Allocation with first-fit and best-fit

// find a suitable free block using the selected policy
static block_t *find_fit(size_t need)
{
    block_t *b;
    block_t *best = NULL;

    // First-fit: return the first suitable block
    if (current_policy == FIT_FIRST) {
        for (b = head; b != NULL; b = b->next) {
            if (b->free && b->size >= need) {
                return b;
            }
        }

        return NULL;
    }

    // Best-fit: search the entire list for the smallest suitable block
    if (current_policy == FIT_BEST) {
        for (b = head; b != NULL; b = b->next) {
            if (b->free && b->size >= need) {
                if (best == NULL || b->size < best->size) {
                    best = b;
                }
            }
        }

        return best;
    }

    return NULL;
}

// split a large block into an allocated block and a free remainder
static void split_block(block_t *b, size_t need)
{
    block_t *rest;

    // only split if the remainder can hold a header and 16 bytes
    if (b->size < need + HDR + ALIGNMENT) {
        return;
    }

    // calculate the new header address using byte arithmetic
    rest = (block_t *)((char *)payload(b) + need);

    rest->size  = b->size - need - HDR;
    rest->next  = b->next;
    rest->free  = 1;
    rest->magic = MAGIC;

    b->size = need;
    b->next = rest;

    if (tail == b) {
        tail = rest;
    }
}

// allocate memory and return a 16-byte-aligned pointer
void *my_malloc(size_t size)
{
    block_t *b;
    size_t need;

    if (size == 0) {
        return NULL;
    }

    // prevent overflow when rounding up
    if (size > SIZE_MAX - (ALIGNMENT - 1)) {
        return NULL;
    }

    need = align_up(size);

    b = find_fit(need);

    if (b == NULL) {
        if (grow(need) == NULL) {
            return NULL;
        }

        b = find_fit(need);

        if (b == NULL) {
            return NULL;
        }
    }

    split_block(b, need);

    b->free = 0;

    return payload(b);
}

// Freeing and coalescing

// print an error message using write()
static void report(const char *msg, size_t len)
{
    ssize_t r = write(STDERR_FILENO, msg, len);
    (void)r;
}

// merge physically adjacent free blocks
static void coalesce(void)
{
    block_t *b = head;
    block_t *n;

    while (b != NULL && b->next != NULL) {
        n = b->next;

        // list neighbours are only merged if they touch in memory
        if (b->free && n->free &&
            (char *)payload(b) + b->size == (char *)n) {

            b->size += HDR + n->size;
            b->next = n->next;

            n->magic = 0;

            if (tail == n) {
                tail = b;
            }

            // stay on b to check whether another block can be merged
        } else {
            b = b->next;
        }
    }
}

// find the header belonging to a user pointer
static block_t *find_block(void *ptr)
{
    block_t *b;

    if (head == NULL ||
        (uintptr_t)ptr % ALIGNMENT != 0 ||
        (char *)ptr < (char *)payload(head) ||
        (char *)ptr >= (char *)payload(tail) + tail->size) {
        return NULL;
    }

    b = header(ptr);

    if (b->magic != MAGIC) {
        return NULL;
    }

    return b;
}

// free a block
void my_free(void *ptr)
{
    block_t *b;

    if (ptr == NULL) {
        return;
    }

    b = find_block(ptr);

    if (b == NULL) {
        report(MSG_INVALID, sizeof(MSG_INVALID) - 1);
        return;
    }

    if (b->free) {
        report(MSG_DOUBLE, sizeof(MSG_DOUBLE) - 1);
        return;
    }

    b->free = 1;

    coalesce();
}

// calloc and realloc


// allocate nmemb * size bytes and set them to zero
void *my_calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *p;

    // detect multiplication overflow
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL;
    }

    total = nmemb * size;

    p = my_malloc(total);

    if (p == NULL) {
        return NULL;
    }

    memset(p, 0, total);

    return p;
}

// resize an allocated block while preserving its contents
void *my_realloc(void *ptr, size_t size)
{
    block_t *b;
    block_t *next;
    size_t need;
    size_t old_size;
    size_t copy;
    void *new_ptr;

    // realloc(NULL, n) behaves like malloc(n)
    if (ptr == NULL) {
        return my_malloc(size);
    }

    // realloc(p, 0) frees the block
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    b = find_block(ptr);

    if (b == NULL || b->free) {
        report(MSG_REALLOC, sizeof(MSG_REALLOC) - 1);
        return NULL;
    }

    if (size > SIZE_MAX - (ALIGNMENT - 1)) {
        return NULL;
    }

    need = align_up(size);

    // case 1: the current block is already large enough
    if (need <= b->size) {
        old_size = b->size;

        split_block(b, need);

        if (b->size != old_size) {
            coalesce();
        }

        return ptr;
    }

    // case 2: absorb the next free, physically adjacent block
    next = b->next;

    if (next != NULL && next->free &&
        (char *)payload(b) + b->size == (char *)next &&
        b->size + HDR + next->size >= need) {

        b->size += HDR + next->size;
        b->next = next->next;

        next->magic = 0;

        if (tail == next) {
            tail = b;
        }

        split_block(b, need);

        return ptr;
    }

    // case 3: allocate a new block and copy the old contents
    new_ptr = my_malloc(size);

    if (new_ptr == NULL) {
        return NULL;
    }

    copy = b->size < size ? b->size : size;

    memcpy(new_ptr, ptr, copy);

    my_free(ptr);

    return new_ptr;
}

// Task 7: Statistics, heap dump, policy and heap consistency

// fill in the heap statistics by walking the block list once
void heap_stats(heap_stats_t *out)
{
    block_t *b;

    if (out == NULL) {
        return;
    }

    // initialise all statistics
    out->heap_bytes   = heap_total;
    out->used_bytes   = 0;
    out->free_bytes   = 0;
    out->blocks       = 0;
    out->free_blocks  = 0;
    out->largest_free = 0;

    // walk through the block list
    for (b = head; b != NULL; b = b->next) {
        out->blocks++;

        if (b->free) {
            out->free_bytes += b->size;
            out->free_blocks++;

            if (b->size > out->largest_free) {
                out->largest_free = b->size;
            }
        } else {
            out->used_bytes += b->size;
        }
    }
}

// print every block on its own line
void heap_dump(void)
{
    block_t *b;
    size_t index = 0;
    char line[128];
    int len;
    const char *status;

    for (b = head; b != NULL; b = b->next) {
        status = b->free ? "FREE" : "USED";

        // format the block information into a local buffer
        len = snprintf(line, sizeof(line),
                       "[%2zu] %p size=%zu %s\n",
                       index, (void *)b, b->size, status);

        // write the formatted line to standard output
        if (len > 0) {
            if ((size_t)len >= sizeof(line)) {
                len = (int)sizeof(line) - 1;
            }

            (void)write(STDOUT_FILENO, line, (size_t)len);
        }

        index++;
    }
}

// select the allocation policy
void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST) {
        current_policy = p;
    }
}

// check whether the heap's block list is consistent
int heap_check(void)
{
    block_t *b;
    block_t *prev = NULL;

    for (b = head; b != NULL; b = b->next) {

        // rule 1: every block must have a valid magic value
        if (b->magic != MAGIC) {
            return 0;
        }

        // rule 2: every payload must be 16-byte aligned
        if ((uintptr_t)payload(b) % ALIGNMENT != 0) {
            return 0;
        }

        if (prev != NULL) {

            // rule 3: blocks must appear in increasing address order
            if ((uintptr_t)b <= (uintptr_t)prev) {
                return 0;
            }

            // rule 4: physically adjacent free blocks must be merged
            if (prev->free && b->free &&
                (char *)payload(prev) + prev->size == (char *)b) {
                return 0;
            }
        }

        prev = b;
    }

    // the last block in the list must be tail
    if (prev != tail) {
        return 0;
    }

    return 1;
}
