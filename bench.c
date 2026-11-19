
// bench.c - Compare FIT_FIRST, FIT_BEST and the standard C allocator

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myalloc.h"

#define OPERATIONS 50000
#define MAX_SLOTS 1024
#define MIN_SIZE 1
#define MAX_SIZE 4096
#define RANDOM_SEED 12345

// Each operation stores the action, memory slot and requested allocation size.
typedef struct {
    int allocate;
    int slot;
    size_t size;
} operation_t;

// Static arrays avoid calling malloc while preparing the workload.
static operation_t operations[OPERATIONS];
static void *slots[MAX_SLOTS];
static unsigned char occupied[MAX_SLOTS];

// Generate one reproducible sequence of valid allocation and free operations.
static void generate_workload(void)
{
    srand(RANDOM_SEED);

    for (int i = 0; i < MAX_SLOTS; i++) {
        occupied[i] = 0;
    }

    for (int i = 0; i < OPERATIONS; i++) {
        int slot = rand() % MAX_SLOTS;

        operations[i].slot = slot;
        operations[i].size = MIN_SIZE + (size_t)(rand() % (MAX_SIZE - MIN_SIZE + 1));

        if (occupied[slot]) {
            operations[i].allocate = 0;
            occupied[slot] = 0;
        } else {
            operations[i].allocate = 1;
            occupied[slot] = 1;
        }
    }
}

// Return the elapsed time between two clock readings in milliseconds.
static double elapsed_ms(struct timespec start, struct timespec end)
{
    double seconds = (double)(end.tv_sec - start.tv_sec);
    double nanoseconds = (double)(end.tv_nsec - start.tv_nsec);

    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

// Clear the pointer slots before starting a new benchmark run.
static void clear_slots(void)
{
    for (int i = 0; i < MAX_SLOTS; i++) {
        slots[i] = NULL;
    }
}

// Run the workload using the selected custom allocator policy.
static double run_custom_allocator(fit_policy_t policy, heap_stats_t *stats)
{
    struct timespec start, end;

    clear_slots();
    set_policy(policy);

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < OPERATIONS; i++) {
        int slot = operations[i].slot;

        if (operations[i].allocate) {
            slots[slot] = my_malloc(operations[i].size);
        } else {
            my_free(slots[slot]);
            slots[slot] = NULL;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    // Capture statistics before freeing the allocations still in use.
    heap_stats(stats);

    // Release remaining allocations so the next policy starts cleanly.
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i] != NULL) {
            my_free(slots[i]);
            slots[i] = NULL;
        }
    }

    return elapsed_ms(start, end);
}

// Run the identical workload using the C library's malloc and free.
static double run_system_allocator(void)
{
    struct timespec start, end;

    clear_slots();

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < OPERATIONS; i++) {
        int slot = operations[i].slot;

        if (operations[i].allocate) {
            slots[slot] = malloc(operations[i].size);
        } else {
            free(slots[slot]);
            slots[slot] = NULL;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    // Free any allocations that remain after the timed workload.
    for (int i = 0; i < MAX_SLOTS; i++) {
        free(slots[i]);
        slots[i] = NULL;
    }

    return elapsed_ms(start, end);
}

// Calculate external fragmentation, avoiding division by zero.
static double fragmentation(size_t free_bytes, size_t largest_free)
{
    if (free_bytes == 0) {
        return 0.0;
    }

    return 1.0 - ((double)largest_free / (double)free_bytes);
}

// Display the measurements collected from a custom allocator run.
static void print_results(const char *name, const heap_stats_t *stats,
                          double time_ms)
{
    printf("\n%s Results\n", name);
    printf("------------------------------\n");
    printf("Heap bytes:          %zu\n", stats->heap_bytes);
    printf("Free bytes:          %zu\n", stats->free_bytes);
    printf("Free blocks:         %zu\n", stats->free_blocks);
    printf("Largest free block:  %zu\n", stats->largest_free);
    printf("External fragmentation: %.4f (%.2f%%)\n",
           fragmentation(stats->free_bytes, stats->largest_free),
           fragmentation(stats->free_bytes, stats->largest_free) * 100.0);
    printf("Execution time:      %.3f ms\n", time_ms);
}

int main(void)
{
    heap_stats_t first_stats;
    heap_stats_t best_stats;

    double first_time;
    double best_time;
    double system_time;

    printf("Memory Allocator Benchmark\n");
    printf("==========================\n");
    printf("Operations: %d\n", OPERATIONS);
    printf("Allocation sizes: %d - %d bytes\n", MIN_SIZE, MAX_SIZE);
    printf("Random seed: %d\n", RANDOM_SEED);

    // Generate the workload once so every allocator receives identical operations.
    generate_workload();

    printf("\nRunning FIT_FIRST...\n");
    first_time = run_custom_allocator(FIT_FIRST, &first_stats);

    printf("Running FIT_BEST...\n");
    best_time = run_custom_allocator(FIT_BEST, &best_stats);

    printf("Running standard malloc/free...\n");
    system_time = run_system_allocator();

    // Print the measurements needed for the Task 8 report.
    print_results("FIT_FIRST", &first_stats, first_time);
    print_results("FIT_BEST", &best_stats, best_time);

    printf("\nStandard C Library malloc/free\n");
    printf("------------------------------\n");
    printf("Execution time: %.3f ms\n", system_time);

    printf("\nBenchmark completed.\n");

    return EXIT_SUCCESS;
}
