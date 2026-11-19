# Memory Allocator Benchmark Report

## 1. Introduction

This experiment compares the performance of two memory allocation placement policies implemented in my custom memory allocator: First-Fit (`FIT_FIRST`) and Best-Fit (`FIT_BEST`). The purpose is to evaluate their memory usage, external fragmentation and execution time, and compare their performance with the standard C library's `malloc()` and `free()` functions.

## 2. Benchmark Methodology

The benchmark program (`bench.c`) performs 50,000 allocation and deallocation operations using allocation sizes ranging from 1 to 4096 bytes. A fixed random seed of `12345` is used to generate the workload, ensuring that both placement policies are tested using the same sequence of operations.

The experiment was performed three times:
- Using the custom allocator with `FIT_FIRST`.
- Using the custom allocator with `FIT_BEST`.
- Using the standard C library's `malloc()` and `free()` functions.

Execution time was measured using `clock_gettime()`. After each custom allocator run, `heap_stats()` was used to collect heap size, free memory, number of free blocks and largest free block.

## 3. Benchmark Results

The following table shows the measurements obtained from running the benchmark.

| Measurement | FIT_FIRST | FIT_BEST |
|---|---:|---:|
| Heap bytes | 1,376,256 | 1,376,256 |
| Free bytes | 273,792 | 274,848 |
| Free blocks | 222 | 169 |
| Largest free block | 115,424 | 109,376 |
| External fragmentation | 57.84% | 60.20% |
| Execution time | 373.284 ms | 447.732 ms |

### External Fragmentation Calculation

External fragmentation is calculated using:

`1 - (largest_free / free_bytes)`

**FIT_FIRST:**

`1 - (115424 / 273792) = 0.5784`

External fragmentation = **57.84%**

**FIT_BEST:**

`1 - (109376 / 274848) = 0.6020`

External fragmentation = **60.20%**

## 4. Execution Time Comparison

The execution times recorded during the benchmark were:

| Allocator | Execution time |
|---|---:|
| Custom allocator (FIT_FIRST) | 373.284 ms |
| Custom allocator (FIT_BEST) | 447.732 ms |
| Standard C library malloc/free | 6.406 ms |

The standard C library allocator completed the workload considerably faster than both custom allocation policies. FIT_FIRST was also faster than FIT_BEST in this experiment.

## 5. Discussion and Conclusion

Based on these results, I would choose the First-Fit policy for this allocator. FIT_FIRST completed the benchmark in 373.284 ms, compared with 447.732 ms for FIT_BEST. It also produced lower external fragmentation (57.84% compared with 60.20%) and a larger largest free block (115,424 bytes compared with 109,376 bytes). Although FIT_BEST had slightly more total free memory and fewer free blocks, these advantages did not result in better fragmentation or execution time in this experiment.

The standard C library's `malloc()` and `free()` were significantly faster, completing the same workload in only 6.406 ms. One reason my custom allocator is slower is that it uses a linked list to search for suitable free blocks. These searches can require visiting many blocks, especially when using Best-Fit, which scans the entire list to find the smallest suitable block. The experiment shows that First-Fit provides a better balance of speed and fragmentation for this particular workload.