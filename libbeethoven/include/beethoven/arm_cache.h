/*
 * Copyright (c) 2024,
 * The University of California, Berkeley and Duke University.
 * All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BEETHOVEN_ARM_CACHE_H
#define BEETHOVEN_ARM_CACHE_H

#include <cstdint>
#include <cstddef>

namespace beethoven {

// ARM64 cache line size (64 bytes standard for ARM Cortex-A)
constexpr size_t ARM_CACHE_LINE_SIZE = 64;

/**
 * ARM64 data cache flush - clean and invalidate by virtual address
 * Ensures CPU-written data reaches RAM before FPGA reads it
 *
 * Uses ARM64 'dc civac' (Data Cache clean and Invalidate by VA to PoC)
 * Only enabled when ENABLE_CACHE_FLUSH is defined
 *
 * @param addr Virtual address of memory region
 * @param size Size in bytes
 */
inline void arm_dcache_flush(void* addr, size_t size) {
#if defined(ENABLE_CACHE_FLUSH) && defined(__aarch64__)
    // dsb ish before: ensure stores prior to this call are observable
    // to the cache controller before we start cleaning lines.
    asm volatile("dsb ish" ::: "memory");
    char* ptr = (char*)addr;
    for (size_t i = 0; i < size; i += ARM_CACHE_LINE_SIZE) {
        asm volatile("dc civac, %0" : : "r"(ptr + i) : "memory");
    }
    // dsb ish after: WAIT for the cache maintenance ops to complete —
    // i.e. for the dirty cache lines to actually reach DDR. A bare
    // dmb (which __sync_synchronize emits) only ORDERS memory accesses;
    // it doesn't wait. Without the dsb the FPGA can race ahead and read
    // stale DDR while the writeback is still in flight, producing
    // all-zero results from any accelerator. (Mirrors what the ARM
    // Linux kernel does in __flush_dcache_area.)
    asm volatile("dsb ish" ::: "memory");
#else
    (void)addr;
    (void)size;
#endif
}

/**
 * ARM64 data cache invalidate - discard cached data
 * Forces CPU to re-read from RAM after FPGA writes
 *
 * Uses ARM64 'dc civac' (Data Cache clean and Invalidate by VA to PoC)
 * Only enabled when ENABLE_CACHE_FLUSH is defined
 *
 * @param addr Virtual address of memory region
 * @param size Size in bytes
 */
inline void arm_dcache_invalidate(void* addr, size_t size) {
#if defined(ENABLE_CACHE_FLUSH) && defined(__aarch64__)
    // Same dsb-ish dance as arm_dcache_flush above. dc civac is used
    // for both directions (it cleans AND invalidates), so this is
    // really "drain anything dirty, then drop the lines so the next
    // load re-fetches from DDR."
    asm volatile("dsb ish" ::: "memory");
    char* ptr = (char*)addr;
    for (size_t i = 0; i < size; i += ARM_CACHE_LINE_SIZE) {
        asm volatile("dc civac, %0" : : "r"(ptr + i) : "memory");
    }
    asm volatile("dsb ish" ::: "memory");
#else
    (void)addr;
    (void)size;
#endif
}

} // namespace beethoven

#endif // BEETHOVEN_ARM_CACHE_H
