/*
 * Copyright (c) 2019,
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

//
// Created by Brendan Sweeney on 2018-06-22.
//

#ifndef RUNTIME_BUTIL_H
#define RUNTIME_BUTIL_H

#include <cstdio>
#include <iomanip>
#include <cmath>
#include <iostream>
#include <cstring>
#include <sys/mman.h>

#include "fpga_transfer.h"
#include "rocc.h"

#define BUFFER_SHARED (1)
const uint64_t maxAddr = 0x400000000LL; //16 GiB

uint64_t staticAvailableAddr = 0x10000000L;//0x0;

uint64_t static_balloc(size_t size) {
    uint64_t toRet = staticAvailableAddr;
    staticAvailableAddr += size;
    if ((staticAvailableAddr & 0x3ff) != 0) {
        staticAvailableAddr = staticAvailableAddr & 0xfffffffffffffc00;
        staticAvailableAddr += 0x400;
    }
    if (toRet >= maxAddr) {
        printf("Allocation of 0x%lx bytes is beyond available memory\n", size);
        exit(-3);
    }
    assert(toRet + size <= staticAvailableAddr);
    return toRet;
}

uint64_t static_balloc(size_t count, size_t size) {
    return static_balloc(count * size);
}


uint64_t availableAddr = 0x0;//0x0;

uint64_t balloc(size_t size) {
    uint64_t toRet = availableAddr;
    availableAddr += size;
    if ((availableAddr & 0x3ff) != 0) {
        availableAddr = availableAddr & 0xfffffffffffffc00;
        availableAddr += 0x400;
    }
    if (toRet >= maxAddr) {
        printf("Allocation of 0x%lx bytes is beyond available memory\n", size);
        exit(-3);
    }
    assert(toRet + size <= availableAddr);
    return toRet;
}

uint64_t balloc(size_t count, size_t size) {
    return balloc(count * size);
}


uint64_t calc_local_buffer_address(uint8_t opcode, uint8_t channel, uint8_t coreId, uint64_t offset) {
    const uint64_t maxCores = 32;
    const uint64_t maxChannels = 4;
    const uint64_t maxBufSize = 1024 * 4 * 4;
    const uint64_t localBufSize = 1024 * 4;
    //might have to bump maxBufSize to * 4, have localBufSize for coreId because wacky addressing
    //BufAddr Opcode: 4 Channel: 0 Low Core: 4: 0000000040810000
    const uint64_t maxMemAddr = 0x40000000L;
    //47400000


    return maxMemAddr + (opcode * maxChannels * maxCores * maxBufSize) + (channel * maxCores * maxBufSize) +
           (coreId / BUFFER_SHARED) * maxBufSize * BUFFER_SHARED +
           ((coreId % BUFFER_SHARED) * localBufSize) + offset;
}

uint64_t calc_consumer_buffer_address(uint8_t opcode, uint8_t channel, uint8_t coreId, uint64_t offset) {
    const uint64_t maxCores = 32;
    const uint64_t maxChannels = 5;
    const uint64_t maxBufSize = 1024 * 4 * 4;
    const uint64_t localBufSize = 1024 * 4 * 4;
    //might have to bump maxBufSize to * 4, have localBufSize for coreId because wacky addressing
    //BufAddr Opcode: 4 Channel: 0 Low Core: 4: 0000000040810000
    const uint64_t maxMemAddr = 0x400000000L;
    //47400000


    return maxMemAddr + (opcode * maxChannels * maxCores * maxBufSize) + (channel * maxCores * maxBufSize) +
           (coreId / BUFFER_SHARED) * maxBufSize * BUFFER_SHARED +
           ((coreId % BUFFER_SHARED) * localBufSize) + offset;
}

template<typename T>
void get_chunk_from_fpga_smart(fpga_handle_t* handle, T* data, uint64_t loc, size_t size) {
    uint32_t* copy_formatted = (uint32_t*) malloc(size);
    get_chunk_from_fpga(handle, copy_formatted, loc, size);
    memcpy(data, copy_formatted, size);
    free(copy_formatted);
}

template<typename T>
void transfer_chunk_to_fpga_smart(fpga_handle_t* handle, T* data, uint64_t loc, size_t size) {

    uint32_t* copy_formatted = (uint32_t*) malloc(size);
    memcpy(copy_formatted, data, size);
    transfer_chunk_to_fpga(handle, copy_formatted, loc, size);
    free(copy_formatted);
}

void stitch_debug_print(fpga_handle_t* handle, uint64_t index, uint64_t a, size_t size) {
//debug printing of joined reads/refs

    auto indexLocal = (uint32_t*) calloc(size, sizeof(uint32_t));
    auto aLocal = (uint8_t*) calloc(size, sizeof(char));
    get_chunk_from_fpga_smart(handle, indexLocal, (uint64_t) index,
                              size * sizeof(uint32_t));
    get_chunk_from_fpga_smart(handle, aLocal, (uint64_t) a,
                              size * sizeof(char));


    for (uint64_t i = 0; i < size; i++) {
        //printf("%4d %08x %02x %02x\n", i, indexLocal[i], aLocal[i], bLocal[i]);
        printf("%4d %08x %c\n", i, indexLocal[i], aLocal[i]);

    }


    free(indexLocal);
    free(aLocal);
}

/**
 * assumes uint32_t, char, char
 * @param handle
 */
void stitch_debug_print(fpga_handle_t* handle, uint64_t index, uint64_t a, uint64_t b, size_t size) {
//debug printing of joined reads/refs

    auto indexLocal = (uint32_t*) calloc(size, sizeof(uint32_t));
    auto aLocal = (uint8_t*) calloc(size, sizeof(char));
    auto bLocal = (uint8_t*) calloc(size, sizeof(char));
    get_chunk_from_fpga_smart(handle, indexLocal, (uint64_t) index,
                              size * sizeof(uint32_t));
    get_chunk_from_fpga_smart(handle, aLocal, (uint64_t) a,
                              size * sizeof(char));
    get_chunk_from_fpga_smart(handle, bLocal, (uint64_t) b,
                              size * sizeof(char));


    for (uint64_t i = 0; i < size; i++) {
        //printf("%4d %08x %02x %02x\n", i, indexLocal[i], aLocal[i], bLocal[i]);
        printf("%4d %08x %c %0c\n", i, indexLocal[i], aLocal[i], bLocal[i]);

    }


    free(indexLocal);
    free(aLocal);
    free(bLocal);
}

void stitch_debug_print(fpga_handle_t* handle, uint64_t index, uint64_t a, uint64_t b, uint64_t c, size_t size) {
//debug printing of joined reads/refs

    auto indexLocal = (uint32_t*) calloc(size, sizeof(uint32_t));
    auto aLocal = (uint8_t*) calloc(size, sizeof(char));
    auto bLocal = (uint8_t*) calloc(size, sizeof(char));
    auto cLocal = (uint8_t*) calloc(size, sizeof(char));

    get_chunk_from_fpga_smart(handle, indexLocal, (uint64_t) index,
                              size * sizeof(uint32_t));
    get_chunk_from_fpga_smart(handle, aLocal, (uint64_t) a,
                              size * sizeof(char));
    get_chunk_from_fpga_smart(handle, bLocal, (uint64_t) b,
                              size * sizeof(char));
    get_chunk_from_fpga_smart(handle, cLocal, (uint64_t) c,
                              size * sizeof(char));


    for (uint64_t i = 0; i < size; i++) {
        //printf("%4d %08x %02x %02x %02x\n", i, indexLocal[i], aLocal[i], bLocal[i], cLocal[i]);
        printf("%4d %08x %c %c %02x\n", i, indexLocal[i], aLocal[i], bLocal[i], cLocal[i]);
    }


    free(indexLocal);
    free(aLocal);
    free(bLocal);
    free(cLocal);
}

template<typename T1>
void varUIntPrint(T1 a) {
    //printf("%i\n", sizeof(T1));
    std::cout << std::setw(sizeof(T1) * 2) << std::setfill('0') << std::hex << +a; //the +a is because stupid
}

template<>
void varUIntPrint<char>(char a) {
    std::cout << a;
}

template<typename T1, typename T2, typename T3, typename T4>
void stitch_debug_print_smart(fpga_handle_t* handle, uint64_t a, uint64_t b, uint64_t c, uint64_t d, size_t size) {
//debug printing of joined reads/refs

    auto aLocal = (T1*) calloc(size, sizeof(T1));
    auto bLocal = (T2*) calloc(size, sizeof(T2));
    auto cLocal = (T3*) calloc(size, sizeof(T3));
    auto dLocal = (T4*) calloc(size, sizeof(T4));

    get_chunk_from_fpga_smart(handle, aLocal, (uint64_t) a,
                              size * sizeof(T1));
    get_chunk_from_fpga_smart(handle, bLocal, (uint64_t) b,
                              size * sizeof(T2));
    get_chunk_from_fpga_smart(handle, cLocal, (uint64_t) c,
                              size * sizeof(T3));
    get_chunk_from_fpga_smart(handle, dLocal, (uint64_t) d,
                              size * sizeof(T4));
    fsync(handle->get_read_fd());


    uint64_t idxSize = 0;

    {
        uint64_t t = size;
        while (t) {
            idxSize++;
            t /= 10;
        }
        if (idxSize == 0) {
            idxSize = 1;
        }
    }


    for (uint64_t i = 0; i < size; i++) {
        //printf("%4d %08x %02x %02x %02x\n", i, aLocal[i], bLocal[i], cLocal[i], dLocal[i]);
        std::cout << std::dec << std::setw(idxSize) << i;
        std::cout << ' ';
        varUIntPrint(aLocal[i]);
        std::cout << ' ';
        varUIntPrint(bLocal[i]);
        std::cout << ' ';
        varUIntPrint(cLocal[i]);
        std::cout << ' ';
        varUIntPrint(dLocal[i]);
        std::cout << '\n';
    }
    //std::cout << std::endl;


    free(aLocal);
    free(bLocal);
    free(cLocal);
    free(dLocal);
    fflush(stdout);
}

template<typename T1, typename T2, typename T3>
void stitch_debug_print_smart(fpga_handle_t* handle, uint64_t a, uint64_t b, uint64_t c, size_t size) {
//debug printing of joined reads/refs

    auto aLocal = (T1*) calloc(size, sizeof(T1));
    auto bLocal = (T2*) calloc(size, sizeof(T2));
    auto cLocal = (T3*) calloc(size, sizeof(T3));

    get_chunk_from_fpga_smart(handle, aLocal, (uint64_t) a,
                              size * sizeof(T1));
    get_chunk_from_fpga_smart(handle, bLocal, (uint64_t) b,
                              size * sizeof(T2));
    get_chunk_from_fpga_smart(handle, cLocal, (uint64_t) c,
                              size * sizeof(T3));
    fsync(handle->get_read_fd());


    uint64_t idxSize = 0;

    {
        uint64_t t = size;
        while (t) {
            idxSize++;
            t /= 10;
        }
        if (idxSize == 0) {
            idxSize = 1;
        }
    }


    for (uint64_t i = 0; i < size; i++) {
        //printf("%4d %08x %02x %02x %02x\n", i, aLocal[i], bLocal[i], cLocal[i], dLocal[i]);
        std::cout << std::dec << std::setw(idxSize) << i;
        std::cout << ' ';
        varUIntPrint(aLocal[i]);
        std::cout << ' ';
        varUIntPrint(bLocal[i]);
        std::cout << ' ';
        varUIntPrint(cLocal[i]);
        std::cout << '\n';
    }
    //std::cout << std::endl;


    free(aLocal);
    free(bLocal);
    free(cLocal);
    fflush(stdout);
}

template<typename T1, typename T2>
void stitch_debug_print_smart(fpga_handle_t* handle, uint64_t a, uint64_t b, size_t size) {
//debug printing of joined reads/refs

    auto aLocal = (T1*) calloc(size, sizeof(T1));
    auto bLocal = (T2*) calloc(size, sizeof(T2));

    get_chunk_from_fpga_smart(handle, aLocal, (uint64_t) a,
                              size * sizeof(T1));
    get_chunk_from_fpga_smart(handle, bLocal, (uint64_t) b,
                              size * sizeof(T2));

    fsync(handle->get_read_fd());


    uint64_t idxSize = 0;

    {
        uint64_t t = size;
        while (t) {
            idxSize++;
            t /= 10;
        }
        if (idxSize == 0) {
            idxSize = 1;
        }
    }


    for (uint64_t i = 0; i < size; i++) {
        //printf("%4d %08x %02x %02x %02x\n", i, aLocal[i], bLocal[i], cLocal[i], dLocal[i]);
        std::cout << std::dec << std::setw(idxSize) << i;
        std::cout << ' ';
        varUIntPrint(aLocal[i]);
        std::cout << ' ';
        varUIntPrint(bLocal[i]);
        std::cout << '\n';
    }
    //std::cout << std::endl;
    free(aLocal);
    free(bLocal);
    fflush(stdout);
}

void write_bytes(const char* filename, const void* bytes, const size_t size) {
    auto file = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (file == -1) {
        printf("Failed to open %s\n", filename);
        exit(-4);
    }
    write(file, bytes, size);
    //auto fbytes = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    //memcpy(fbytes, bytes, size);
    //munmap(fbytes, size);
    close(file);
    fsync(file);
}

void read_bytes(const char* filename, void* bytes, const size_t size) {
    auto file = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (file == -1) {
        printf("Failed to open %s\n", filename);
        exit(-4);
    }
    read(file, bytes, size);
    //auto fbytes = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    //memcpy(bytes, fbytes, size);
    //munmap(fbytes, size);
    close(file);
}

static long long get_elapsed(struct timespec &start, struct timespec &end) {
    long long sec_diff, nsec_diff;
    if (end.tv_sec >= start.tv_sec) {
        sec_diff = end.tv_sec - start.tv_sec;
        if (end.tv_nsec >= start.tv_nsec) {
            nsec_diff = end.tv_nsec - start.tv_nsec;
            return sec_diff * 1000000000LL + nsec_diff;
        } else {
            nsec_diff = start.tv_nsec - end.tv_nsec;
            return sec_diff * 1000000000LL - nsec_diff;
        }
    } else {
        return 0LL;
    }
}

static inline void getcl(struct timespec &toUpdate) {
    clock_gettime(CLOCK_MONOTONIC, &toUpdate);
}

#endif //RUNTIME_BUTIL_H
