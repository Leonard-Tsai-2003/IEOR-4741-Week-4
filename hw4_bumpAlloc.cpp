// g++ -std=c++17 -O2 -Wall -Iinclude hw4_bumpAlloc.cpp -o /tmp/bumpAlloc  && /tmp/bumpAlloc

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>
#include <iostream>

#include "bench.hpp"
#include "benchmark_from_hw2.hpp"
#include "pool.hpp"

// 1. Bump Allocator: Sequential offset bump with standard std::align support
class BumpAllocator {
public:
    explicit BumpAllocator(std::size_t capacity)
        : capacity_(capacity), offset_(0) {
        buffer_ = static_cast<uint8_t*>(::operator new(capacity_));
    }

    ~BumpAllocator() {
        ::operator delete(buffer_);
    }

    // Move-only / non-copyable
    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;

    void* alloc(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
        void* current = buffer_ + offset_;
        std::size_t remaining_space = capacity_ - offset_;

        // std::align adjusts 'current' pointer to match alignment requirements
        if (std::align(alignment, bytes, current, remaining_space)) {
            offset_ = (static_cast<uint8_t*>(current) - buffer_) + bytes;
            return current;
        }
        return nullptr; // Out of memory or alignment failed
    }

    void reset() noexcept {
        offset_ = 0; // O(1) reset: rewind pointer to beginning
    }

    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t bytes_used() const noexcept { return offset_; }

private:
    uint8_t* buffer_;
    std::size_t capacity_;
    std::size_t offset_;
};


// 2. Malloc/Free Batch Allocator: Replicates the same batch lifecycle using std::malloc
class MallocBatchAllocator {
public:
    explicit MallocBatchAllocator(std::size_t initial_reservation) {
        allocations_.reserve(initial_reservation);
    }

    ~MallocBatchAllocator() {
        reset();
    }

    // Move-only / non-copyable
    MallocBatchAllocator(const MallocBatchAllocator&) = delete;
    MallocBatchAllocator& operator=(const MallocBatchAllocator&) = delete;

    void* alloc(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
        (void)alignment; // std::malloc guarantees alignment for any scalar type
        void* ptr = std::malloc(bytes);
        if (ptr) {
            allocations_.push_back(ptr);
        }
        return ptr;
    }

    void reset() noexcept {
        // O(N) reset: free every individual allocation made during the batch
        for (void* ptr : allocations_) {
            std::free(ptr);
        }
        allocations_.clear();
    }

    std::size_t allocation_count() const noexcept { return allocations_.size(); }

private:
    std::vector<void*> allocations_;
};


// 3. Pool Batch Allocator: Fixed-size object pool with free-list recycling
class PoolBatchAllocator {
public:
    PoolBatchAllocator(std::size_t obj_size, std::size_t capacity)
        : pool_(obj_size, capacity) {
        allocations_.reserve(capacity);
    }

    ~PoolBatchAllocator() {
        reset();
    }

    // Move-only / non-copyable
    PoolBatchAllocator(const PoolBatchAllocator&) = delete;
    PoolBatchAllocator& operator=(const PoolBatchAllocator&) = delete;

    void* alloc(std::size_t bytes = 0, std::size_t alignment = alignof(std::max_align_t)) {
        (void)bytes;
        (void)alignment;
        void* ptr = pool_.alloc();
        if (ptr) {
            allocations_.push_back(ptr);
        }
        return ptr;
    }

    void reset() noexcept {
        // Return allocated slots to the free-list
        for (void* ptr : allocations_) {
            pool_.free(ptr);
        }
        allocations_.clear();
    }

    std::size_t allocation_count() const noexcept { return allocations_.size(); }

private:
    Pool pool_;
    std::vector<void*> allocations_;
};

int main() {
    const long ALLOCS_PER_BATCH = 1000;
    const std::size_t ALLOC_SIZE = 64; // 64 bytes per allocation
    const std::size_t POOL_CAPACITY_SLOTS = ALLOCS_PER_BATCH * 2;
    const std::size_t BUMP_CAPACITY = POOL_CAPACITY_SLOTS * ALLOC_SIZE;

    BumpAllocator bump(BUMP_CAPACITY);
    MallocBatchAllocator malloc_alloc(ALLOCS_PER_BATCH);
    PoolBatchAllocator pool_alloc(ALLOC_SIZE, POOL_CAPACITY_SLOTS);

    printf("HW 4 — BumpAllocator vs PoolBatchAllocator vs MallocBatchAllocator\n");
    printf("Allocations per batch: %ld (%zu bytes each)\n\n", ALLOCS_PER_BATCH, ALLOC_SIZE);

    printf("  %-28s %12s %12s %12s %12s\n", "variant", "p50", "p99", "p99.9", "mean");
    printf("  %-28s %12s %12s %12s %12s\n", "", "ns/op", "ns/op", "ns/op", "ns/op");
    printf("  ---------------------------------------------------------------------------------\n");

    // Benchmark BumpAllocator
    Stats bump_stats = bench([&]() {
        for (long i = 0; i < ALLOCS_PER_BATCH; ++i) {
            void* p = bump.alloc(ALLOC_SIZE);
            doNotOptimize(p);
        }
        bump.reset();
    }, ALLOCS_PER_BATCH, 1000, /*warmup =*/ 50);

    // Benchmark PoolBatchAllocator
    Stats pool_stats = bench([&]() {
        for (long i = 0; i < ALLOCS_PER_BATCH; ++i) {
            void* p = pool_alloc.alloc(ALLOC_SIZE);
            doNotOptimize(p);
        }
        pool_alloc.reset();
    }, ALLOCS_PER_BATCH, 1000, /*warmup =*/ 50);

    // Benchmark MallocBatchAllocator
    Stats malloc_stats = bench([&]() {
        for (long i = 0; i < ALLOCS_PER_BATCH; ++i) {
            void* p = malloc_alloc.alloc(ALLOC_SIZE);
            doNotOptimize(p);
        }
        malloc_alloc.reset();
    }, ALLOCS_PER_BATCH, 1000, /*warmup =*/ 50);

    printf("  %-28s %12.3f %12.3f %12.3f %12.3f\n", "PoolBatchAllocator (Pool)", pool_stats.p50, pool_stats.p99, pool_stats.p999, pool_stats.mean);
    printf("  %-28s %12.3f %12.3f %12.3f %12.3f\n", "BumpAllocator (O(1) bump)", bump_stats.p50, bump_stats.p99, bump_stats.p999, bump_stats.mean);
    printf("  %-28s %12.3f %12.3f %12.3f %12.3f\n", "MallocBatchAllocator (malloc)", malloc_stats.p50, malloc_stats.p99, malloc_stats.p999, malloc_stats.mean);

    printf("\n  Speedup (p50 Malloc vs Bump): %.2fx\n", malloc_stats.p50 / bump_stats.p50);
    printf("  Speedup (p50 Malloc vs Pool): %.2fx\n", malloc_stats.p50 / pool_stats.p50);
    printf("  Speedup (p50 Bump vs Pool):   %.2fx\n", bump_stats.p50 / pool_stats.p50);

    return 0;
}