// myalloc.c - my own malloc and free built on top of sbrk
// Task 1: block header and helper functions
// Task 2: growing the heap with sbrk
// Task 3: my_malloc with first-fit
// Task 4: splitting blocks
// Task 5: my_free and coalescing

// needed so that sbrk() is declared (the assignment asks for this before the includes)
#define _DEFAULT_SOURCE

#include <stddef.h>    // size_t
#include <stdint.h>    // uintptr_t, intptr_t, INTPTR_MAX, SIZE_MAX
#include <unistd.h>    // sbrk, write
#include "myalloc.h"   // the API, fit_policy_t and heap_stats_t

#define ALIGNMENT 16          // every payload address must be 16-byte aligned
#define MAGIC     0x4D414C43  // value stored in every header to spot bad pointers
#define CHUNK     ((size_t)64 * 1024)  // ask the OS for at least 64 KB each time

// messages printed by my_free (they end with a newline)
#define MSG_INVALID "my_free: invalid pointer\n"
#define MSG_DOUBLE  "my_free: double free detected\n"

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


static block_t *head = NULL;    // first block in the list
static block_t *tail = NULL;    // last block in the list
static size_t heap_total = 0;   // total bytes obtained from the OS (used for heap_bytes)

// get more memory from the OS and add it to the end of the list as one free block

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

static void split_block(block_t *b, size_t need)
{
    block_t *rest;   // header of the new free block made from the leftover space

    // the leftover must fit a header plus at least 16 bytes of payload, if not give the whole block
    if (b->size < need + HDR + ALIGNMENT) {
        return;
    }

    // the new header starts right after the first part's payload (char * so we count bytes)
    rest = (block_t *)((char *)payload(b) + need);
    rest->size  = b->size - need - HDR;   // what is left after the first part and the new header
    rest->next  = b->next;                // it takes b's place in the list
    rest->free  = 1;
    rest->magic = MAGIC;

    // shrink b to exactly need bytes and link the new block after it
    b->size = need;
    b->next = rest;

    // if b was the last block then the new block is now the last one
    if (tail == b) {
        tail = rest;
    }
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

    // cut off the unused part so we do not waste it (does nothing if the leftover is too small)
    split_block(b, need);

    // mark it as in use and give the user the payload, not the header
    b->free = 0;
    return payload(b);
}



// print a message on stderr with write(), printf is not allowed inside the allocator
static void report(const char *msg, size_t len)
{
    ssize_t r = write(STDERR_FILENO, msg, len);
    (void)r;   // nothing useful to do if even stderr fails
}

// merge every run of free blocks that sit right next to each other in memory
static void coalesce(void)
{
    block_t *b = head;
    block_t *n;

    while (b != NULL && b->next != NULL) {
        n = b->next;

        // both must be free and b's payload must end exactly where n's header begins
        // (list neighbours are not always memory neighbours, libc malloc can leave gaps)
        if (b->free && n->free && (char *)payload(b) + b->size == (char *)n) {
            b->size += HDR + n->size;   // b now also covers n's header and payload
            b->next  = n->next;         // unlink n from the list
            n->magic = 0;               // so an old pointer to n is seen as invalid later

            // if n was the last block then b is the last block now
            if (tail == n) {
                tail = b;
            }

            // stay on b, the block after it may be free and mergeable too
        } else {
            b = b->next;
        }
    }
}

// give a block back, a NULL pointer is ignored, bad pointers and double frees are reported
void my_free(void *ptr)
{
    block_t *b;

    // free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // the pointer must be aligned and inside our heap, otherwise even reading its header could crash
    if (head == NULL ||
        (uintptr_t)ptr % ALIGNMENT != 0 ||
        (char *)ptr < (char *)payload(head) ||
        (char *)ptr >= (char *)payload(tail) + tail->size) {
        report(MSG_INVALID, sizeof(MSG_INVALID) - 1);
        return;
    }

    b = header(ptr);

    // a wrong magic number means this pointer did not come from my_malloc
    if (b->magic != MAGIC) {
        report(MSG_INVALID, sizeof(MSG_INVALID) - 1);
        return;
    }

    // freeing a block that is already free is a double free
    if (b->free) {
        report(MSG_DOUBLE, sizeof(MSG_DOUBLE) - 1);
        return;
    }

    // mark it free, then merge it with any free neighbours (scans the whole list, so O(n))
    b->free = 1;
    coalesce();
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