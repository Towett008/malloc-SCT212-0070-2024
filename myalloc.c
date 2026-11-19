// myalloc.c - my own malloc and free built on top of sbrk
// Task 1: block header and helper functions
// Task 2: growing the heap with sbrk
// Task 3: my_malloc with first-fit

// needed so that sbrk() is declared (the assignment asks for this before the includes)
#define _DEFAULT_SOURCE

#include <stddef.h>    // size_t
#include <stdint.h>    // uintptr_t, intptr_t, INTPTR_MAX, SIZE_MAX
#include <unistd.h>    // sbrk
#include "myalloc.h"   // the API, fit_policy_t and heap_stats_t

#define ALIGNMENT 16          // every payload address must be 16-byte aligned
#define MAGIC     0x4D414C43  // value stored in every header to spot bad pointers
#define CHUNK     ((size_t)64 * 1024)  // ask the OS for at least 64 KB each time

// the header that sits in front of every payload
typedef struct block {
    size_t        size;   // payload size in bytes
    struct block *next;   // next block in the list (increasing address order)
    int           free;   // 1 = free, 0 = in use
    int           magic;  // always MAGIC, my_free uses it to detect an invalid pointer
} block_t;

// header size rounded up to a multiple of 16 (this is the size used for all pointer math)
#define HDR ((sizeof(block_t) + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1))

// stop the build if HDR is ever not a multiple of 16
_Static_assert(HDR % ALIGNMENT == 0, "HDR must be a multiple of 16");

// round n up to the next multiple of 16 without using division
static inline size_t align_up(size_t n)
{
    // add 15, then clear the last 4 bits, so 1 -> 16, 16 -> 16 and 17 -> 32
    return (n + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
}

// go from a block header to the pointer we give to the user
static inline void *payload(block_t *b)
{
    // cast to char * first so the addition counts bytes and not block_t sizes
    return (char *)b + HDR;
}

// go back from the user's pointer to the header in front of it
static inline block_t *header(void *ptr)
{
    // the header starts exactly HDR bytes before the payload
    return (block_t *)((char *)ptr - HDR);
}

// ---------------------------------------------------------------
// The list of blocks and growing the heap
// ---------------------------------------------------------------

static block_t *head = NULL;    // first block in the list
static block_t *tail = NULL;    // last block in the list
static size_t heap_total = 0;   // total bytes obtained from the OS (used for heap_bytes)

// get more memory from the OS and add it to the end of the list as one free block
// need is the payload size wanted, the new block is at least 64 KB
// returns the new block, or NULL if the OS has no more memory
static block_t *grow(size_t need)
{
    size_t   total;   // size of the new block including its header
    size_t   pad;     // padding bytes so the new block starts on a 16-byte boundary
    void    *cur;     // the current program break
    void    *old;     // the old break returned by sbrk, where our new memory starts
    block_t *b;       // the new block

    // a huge need would overflow when we add the header and padding, so refuse it
    if (need > (size_t)INTPTR_MAX / 2) {
        return NULL;
    }

    // the block needs room for the header and the payload, but never less than 64 KB
    total = HDR + need;
    if (total < CHUNK) {
        total = CHUNK;
    }

    // keep the size a multiple of 16 so the next block also starts aligned
    total = align_up(total);

    // read the current break without moving it, because it may not be aligned
    cur = sbrk(0);
    if (cur == (void *)-1) {
        return NULL;
    }

    // padding needed to reach the next multiple of 16 (0 if it is already aligned)
    pad = (ALIGNMENT - ((uintptr_t)cur % ALIGNMENT)) % ALIGNMENT;

    // move the break up, sbrk returns the old break which is the start of the new memory
    old = sbrk((intptr_t)(pad + total));
    if (old == (void *)-1) {
        return NULL;
    }

    // the block starts after the padding, so its payload is 16-byte aligned
    b = (block_t *)((char *)old + pad);
    b->size  = total - HDR;   // payload size is everything except the header
    b->next  = NULL;          // it will be the last block in the list
    b->free  = 1;             // new memory starts out free
    b->magic = MAGIC;

    // add it to the end of the list
    if (tail == NULL) {
        head = b;             // the list was empty, so this is the first block
    } else {
        tail->next = b;
    }
    tail = b;

    // remember how many bytes we asked the OS for
    heap_total += pad + total;

    return b;
}

// ---------------------------------------------------------------
// Allocation with first-fit
// ---------------------------------------------------------------

// walk the list from the start and return the first free block that is big enough
// need must already be rounded up to a multiple of 16, returns NULL if no block fits
static block_t *find_fit(size_t need)
{
    block_t *b;

    for (b = head; b != NULL; b = b->next) {
        // the block must be free and its payload must hold at least need bytes
        if (b->free && b->size >= need) {
            return b;
        }
    }

    return NULL;   // no free block is big enough
}

// give the user at least size bytes, 16-byte aligned, or NULL if size is 0 or memory runs out
void *my_malloc(size_t size)
{
    block_t *b;
    size_t   need;

    // a request for 0 bytes returns NULL
    if (size == 0) {
        return NULL;
    }

    // rounding up a huge size would wrap around to a small number, so refuse it
    if (size > SIZE_MAX - (ALIGNMENT - 1)) {
        return NULL;
    }

    // round up so every block size stays a multiple of 16
    need = align_up(size);

    b = find_fit(need);

    if (b == NULL) {
        // nothing fits, so get more memory from the OS and try again
        if (grow(need) == NULL) {
            return NULL;   // the OS has no more memory
        }
        b = find_fit(need);
        if (b == NULL) {
            return NULL;   // should not happen, but never return a bad pointer
        }
    }

    // mark it as in use and give the user the payload, not the header
    // (the whole block is handed out for now, splitting comes in Task 4)
    b->free = 0;
    return payload(b);
}


// stub, real version comes in Task 5
void my_free(void *ptr)
{
    (void)ptr;   // not used yet
}

// stub, real version comes in Task 6
void *my_calloc(size_t nmemb, size_t size)
{
    (void)nmemb;
    (void)size;
    return NULL;
}

// stub, real version comes in Task 6
void *my_realloc(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    return NULL;
}

// stub, real version comes in Task 7
void heap_stats(heap_stats_t *out)
{
    // report an empty heap for now
    out->heap_bytes   = 0;
    out->used_bytes   = 0;
    out->free_bytes   = 0;
    out->blocks       = 0;
    out->free_blocks  = 0;
    out->largest_free = 0;
}

// stub, real version comes in Task 7
void heap_dump(void)
{
    // nothing to print yet
}

// stub, real version comes in Task 7
void set_policy(fit_policy_t p)
{
    (void)p;   // not used yet
}

// stub, real version comes in Task 7
int heap_check(void)
{
    // say the heap is fine for now, the real checks are written in Task 7
    return 1;
}