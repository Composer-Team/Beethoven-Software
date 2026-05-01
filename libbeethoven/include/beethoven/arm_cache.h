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
    __sync_synchronize();
    char* ptr = (char*)addr;
    for (size_t i = 0; i < size; i += ARM_CACHE_LINE_SIZE) {
        asm volatile("dc civac, %0" : : "r"(ptr + i) : "memory");
    }
    __sync_synchronize();
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
    __sync_synchronize();
    char* ptr = (char*)addr;
    for (size_t i = 0; i < size; i += ARM_CACHE_LINE_SIZE) {
        asm volatile("dc civac, %0" : : "r"(ptr + i) : "memory");
    }
    __sync_synchronize();
#else
    (void)addr;
    (void)size;
#endif
}

} // namespace beethoven

#endif // BEETHOVEN_ARM_CACHE_H
