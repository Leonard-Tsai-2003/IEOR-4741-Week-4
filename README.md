# IEOR 4741 - Week 4: High-Performance C++ Memory Management & Polymorphism

This repository contains C++ implementations and performance benchmarks for custom memory allocators (free-list pool allocator, bump/arena allocator) and runtime polymorphism versus direct call overheads.

---

## Table of Contents
1. [Free-List Pool Allocator](#1-free-list-pool-allocator)
2. [Bump / Arena Allocator](#2-bump--arena-allocator)
3. [Runtime Polymorphism](#3-runtime-polymorphism)
4. [Virtual Call Cost Benchmark](#4-virtual-call-cost-benchmark)

---

## 1. Free-List Pool Allocator

A high-performance fixed-size memory pool allocator utilizing a singly-linked free list to achieve $O(1)$ allocations and deallocations without dynamic memory allocation overhead.

### Build & Run
```bash
g++ -std=c++17 -O2 -pthread -Iinclude hw4_pool_test.cpp -o /tmp/pool && /tmp/pool
```

### Output
```text
RESULT|pool_basic|pass|4 distinct, writable, non-overlapping slots
RESULT|pool_full|pass|3 distinct slots, then alloc past capacity -> null
RESULT|pool_reuse|pass|freed slot reused
RESULT|pool_pattern|pass|100k fill/drain rounds OK
METRIC|pool_ns_per_op|0.329417
```

---

## 2. Bump / Arena Allocator

A comparative performance evaluation between a fixed-size Pool Allocator, an $O(1)$ Bump/Arena Allocator, and standard system `malloc`.

### Build & Run
```bash
g++ -std=c++17 -O2 -Wall -Iinclude hw4_bumpAlloc.cpp -o /tmp/bumpAlloc && /tmp/bumpAlloc
```

### Benchmark Results
```text
Allocations per batch: 1000 (64 bytes each)

 variant                         p50          p99        p99.9         mean
                                ns/op        ns/op        ns/op        ns/op
 ---------------------------------------------------------------------------------
 PoolBatchAllocator (Pool)           2.792       22.125       43.583        3.429
 BumpAllocator (O(1) bump)           5.791       27.167       57.042        6.790
 MallocBatchAllocator (malloc)       21.583       56.875       85.875       24.288

 Speedup (p50 Malloc vs Bump): 3.73x
 Speedup (p50 Malloc vs Pool): 7.73x
 Speedup (p50 Bump vs Pool):   2.07x
```

### Performance Report
- **Benchmarking Methodology**: Measurements were collected using high-resolution timers provided in `include/bench.hpp`.
- **Run Setup**: Benchmarks include an initial warm-up phase to ensure cache lines and instruction caches are populated, eliminating cold-start bias. Results represent the median (`p50`), 99th percentile (`p99`), 99.9th percentile (`p99.9`), and arithmetic mean across multiple repeated runs.
- **Environment**: Apple M2, RAM: 8 GB, GCC 7+ compiled with `-O2` optimization flags.

---

## 3. Runtime Polymorphism

An implementation verifying strategy-pattern risk checks and object tracking during execution, ensuring clean resource destruction and zero memory leaks upon teardown.

### Build & Run
```bash
g++ -std=c++17 -O2 -Iinclude hw4_polymorphism.cpp -o /tmp/w4_strat && /tmp/w4_strat
```

### Output
```text
risk check status: PASS
risk check status: PASS
live objects after teardown: 0
```

---

## 4. Virtual Call Cost Benchmark

Microbenchmarks evaluating the CPU-level overhead of direct function calls, virtual calls with single versus multiple targets, and devirtualized calls via `final`.

### Build & Run
```bash
g++ -std=c++17 -O2 -Iinclude hw4_virtualCallCost.cpp -o /tmp/w4_vbench && /tmp/w4_vbench
```

### Benchmark Results
```text
direct call (no base class)      0.96 ns/call
virtual, 1 target                1.14 ns/call
virtual, 4 targets, i&3          2.32 ns/call
virtual, 4 targets, random       6.94 ns/call
final class, via R0*             0.89 ns/call
sink=1.20313e+08
```

### Performance Analysis: Indirect Branches, BTB, and Inlining

The benchmark demonstrates a clear latency gap ranging from **0.89 ns** up to **6.94 ns per call**. Virtual function calls incur higher latency because they rely on an **indirect branch** instruction (e.g., `call *%rax`), requiring a pointer lookup in a virtual method table (vtable) rather than jumping to a static memory offset. When a virtual call site consistently resolves to a single target or a predictable sequence (`i & 3`), the CPU's **Branch Target Buffer (BTB)** accurately predicts the jump destination, keeping execution fast (~1.14–2.32 ns/call). However, when call targets are unpredictable/random, the BTB suffers frequent mispredictions, forcing CPU pipeline stalls and flushes that push latency to 6.94 ns/call (~7x slower than a direct call). 

Furthermore, standard virtual calls **cannot be inlined by the compiler** because inlining requires knowing the exact function implementation statically at compile time. Because dynamic polymorphism defers destination resolution to runtime based on the object instance, the compiler must emit an indirect call instruction unless static devirtualization can be proven—such as when a target class or method is marked `final` (reducing latency to 0.89 ns/call).