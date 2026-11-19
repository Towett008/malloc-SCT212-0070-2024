// myalloc.c - my own malloc and free built on top of sbrk
// Task 1: block header and helper functions

// needed so that sbrk() is declared (the assignment asks for this before the includes)
#define _DEFAULT_SOURCE

#include <stddef.h>    // size_t
#include "myalloc.h"   // the API, fit_policy_t and heap_stats_t

#define ALIGNMENT 16          // every payload address must be 16-byte aligned
#define MAGIC     0x4D414C43  // value stored in every header to spot bad pointers

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


void *my_malloc(size_t size)
{
    (void)size;   // not used yet
    return NULL;
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
    // return 0 so the tests do not pass by accident before this is written
    return 0;
}