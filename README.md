# Systems Programming: Custom Memory Allocator

**Name:** Cynthia Chepkorir
**Student ID:** SCT212-0070/2024

## 1. Project Description

This project implements a custom memory allocator in C using the `sbrk()` system call to request memory from the operating system. It provides allocation and deallocation functions similar to the standard C library, including `my_malloc()`, `my_free()`, `my_calloc()` and `my_realloc()`.

The allocator manages memory using block headers and a linked list to keep track of allocated and free memory. It supports First-Fit and Best-Fit placement policies, block splitting, coalescing, heap statistics and heap inspection.

## 2. Building and Running

### Requirements
- Linux or WSL
- GCC compiler
- GNU Make
- Git

### Clone the repository

```bash
git clone https://github.com/Towett008/malloc_SCT212_0070_2024.git
cd malloc_SCT212_0070_2024
```

### Build and run the tests

```bash
make
```

This compiles `myalloc.c` together with `test_myalloc.c`, creates the `test_myalloc` executable and automatically runs the test suite.

### Run the benchmark

```bash
gcc -std=c11 -Wall -Wextra -g -O0 -o bench bench.c myalloc.c
./bench
```

The benchmark compares First-Fit, Best-Fit and the standard C library's `malloc()` and `free()` functions.

### Run with sanitizers

```bash
gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
./t_asan
```

### Remove compiled files

```bash
make clean
rm -f bench t_asan
```



## 3. Implementation Checklist

The following table summarizes the implementation tasks completed in this project.

| Task | Requirement | Status |
|---|---|---|
| Task 1 | Block header and heap initialization | Complete |
| Task 2 | Basic memory allocation using `sbrk()` | Complete |
| Task 3 | First-Fit allocation | Complete |
| Task 4 | Splitting free blocks | Complete |
| Task 5 | Freeing memory and coalescing adjacent blocks | Complete |
| Task 6 | Implementing `my_calloc()` and `my_realloc()` | Complete |
| Task 7 | Best-Fit, heap statistics, heap dump and heap check | Complete |
| Task 8 | Benchmark experiment and report | Complete |

## 4. Final Test Results

The allocator was tested using the provided test harness, which checks basic allocation, memory alignment, block reuse, splitting, coalescing, `calloc`, `realloc`, heap growth, placement policies and stress testing.

The final test suite was run using:

```bash
make
```


```text
T6 heap growth via sbrk
  PASS  large request succeeds
  PASS  heap grew
  PASS  heap consistent
T7 best-fit vs first-fit
  PASS  first-fit takes the first hole
  PASS  best-fit takes the tightest hole
T8 randomized stress test
  PASS  no data corruption during 20000 operations
  PASS  everything freed
  PASS  fully coalesced (few free blocks remain)
  PASS  heap consistent

Result: 29 passed, 0 failed
```



## 5. Design Explanation

The allocator uses a block header to store information about each memory block. The header contains the block's payload size, a pointer to the next block, a flag indicating whether the block is free or allocated, and a magic value used for basic pointer validation. The header size is aligned to 16 bytes so that the payload following it can also maintain the required alignment.

Memory is requested from the operating system using `sbrk()` when the allocator cannot find a suitable free block. The allocator organizes its blocks in a linked list, allowing it to keep track of allocated and free memory. Each block's header is followed by its payload, and the next block is linked through the header's pointer.

When `my_malloc()` is called, the requested size is rounded up to the required alignment. The allocator searches the list for a suitable free block using the selected placement policy. First-Fit selects the first block large enough to satisfy the request, while Best-Fit searches the entire list and selects the smallest suitable block. If a free block is considerably larger than required, it can be split into an allocated block and a smaller free block, provided the remaining space can hold another header and the minimum payload.

When `my_free()` is called, the block is marked as free. The allocator then checks whether physically adjacent free blocks can be merged. Coalescing combines adjacent free blocks into a larger block and updates the linked list. This reduces external fragmentation and makes larger allocations easier to satisfy.

## 6. Questions

### Question 1: Why must the header size be a multiple of 16 as well as the payload sizes?

The payload must be aligned to a 16-byte boundary to satisfy the allocator's alignment requirement. Even if the payload size is rounded up to a multiple of 16, an incorrectly sized header could cause the next payload address to become misaligned. Making the header size a multiple of 16 ensures that the payload following it also starts at a properly aligned address.

### Question 2: Why can `printf()` inside `my_malloc()` cause infinite recursion?

`printf()` may internally request memory from the C library's memory allocator. If `my_malloc()` uses `printf()` for debugging, this can cause another allocation request while the original allocation is still running. This may result in recursive calls and potentially an infinite recursion loop. Using `snprintf()` with `write()` avoids relying on the standard output functions that may allocate memory.

### Question 3: Your `my_free()` is O(n). Which extra field would let you find a block's previous neighbor in O(1), and what is the name of the technique?

Adding a pointer to the previous block in each header would allow the allocator to move backwards through the list in constant time. This would turn the singly linked list into a doubly linked list. Another common technique is boundary tags, which store block-size information at both the beginning and end of a block to help locate neighboring blocks.

### Question 4: Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.

Internal fragmentation occurs when an allocated block contains more memory than the program requested, leaving some unused space inside the block. In this allocator, it can happen when a request is rounded up to a 16-byte boundary or when a block is too small to split. External fragmentation occurs when free memory is divided into separate smaller blocks, making it difficult to satisfy a large allocation even when the total free memory is sufficient. It can happen when blocks are freed in an order that leaves separated free regions.

### Question 5: Why does `sbrk()` memory never get returned to the OS in your allocator? Describe how you could give back memory at the end of the heap.

The allocator does not return memory to the operating system because it does not implement a mechanism to shrink the program break after freeing blocks. Instead, freed memory is kept in the linked list so that it can be reused by future allocation requests. Memory at the end of the heap could be returned by using `brk()` or a negative `sbrk()` adjustment to move the program break backwards, provided the final block is free and is at the current end of the heap.

### Question 6: Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?

This can happen when the allocator obtains additional memory using `sbrk()` but the new memory region is not physically adjacent to the previous region. The linked list can still connect the blocks even though there is a gap between their addresses. The coalescing code checks the actual memory addresses before merging blocks, so it only combines free blocks that are physically adjacent.

## 7. Known Limitations

- The allocator uses a linked list, meaning allocation and deallocation searches can take O(n) time as the number of blocks increases.
- The allocator does not implement thread-safe memory allocation.
- Memory returned through `sbrk()` is retained for reuse rather than automatically returned to the operating system when blocks are freed.
- The allocator is a learning implementation and does not include all the performance optimizations and advanced memory-management features found in production allocators such as glibc.

## 8. Sources and AI-Tool Declaration

### Sources

- ICS 2305 Systems Programming Practical Notes

- C library documentation for memory allocation, memory operations and timing functions.

### AI-tool declaration

ChatGPT was used as a learning and development support tool during this assignment. It assisted with explaining the requirements, understanding memory allocation concepts, reviewing implementation steps, debugging guidance, and preparing the benchmark report and documentation.

The implementation was developed and tested incrementally, with changes committed to the GitHub repository throughout the assignment.

