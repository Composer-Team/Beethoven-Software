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

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <array>
#include <vector>
#include <list>
#include <iostream>
#include <cassert>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdint>
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>
#include <bitset>
#include "../include/fpga_transfer.h"
#include "../include/butil.h"
#include "../include/memrocc.h"
extern "C" {
  #include "../include/gemmNcubed/gemm.h"
  #include "../include/mach_common/support.h"
  #include "../include/gemmNcubed/local_support.h"
}
using namespace std;

#define LEN_QUAL 160
#define NCORE 1
//#define LINENUM 80000000
//#define LINENUM 80
#define INPUT_SIZE sizeof(bench_args_t)
//#define TYPE uint32_t
#define N 9

#define MAXCHAR 1000

static long get_elapsed(struct timespec* start, struct timespec* end);
void help_text() {
    printf("Wrong input\n");
}


// Parameter mapping
void runGemmNcubed(simif_f1_t *handle, int core, 
                      uint64_t inputAddrs1, uint64_t inputAddrs2, uint64_t outputAddr) {
    printf("Setting up gemm ncubed unit...\n");
    
    /*
     * Gemm Ncubed has two read channels and one write channel
     */
    gemm_set_addr(handle, 0, (void*) inputAddrs1, core); //read channel 0
    gemm_set_addr(handle, 1, (void*) inputAddrs2, core); //read channel 1
    gemm_set_addr(handle, 2, (void*) outputAddr, core); //write channel 0
    gemm_start(handle, core, N);
    printf("Finished setting up gemm ncbued unit \n");
}


struct quality_struct {
    char qual[LEN_QUAL];
};
typedef struct quality_struct qualStruct;

void handlefield(char* fbegin, uint64_t* i, char* loc) {
    uint64_t offsetlength = 0;
    while (fbegin[*i] != '|') {
        offsetlength++;
        (*i)++;
    }
    memcpy(loc, &fbegin[*i - offsetlength], (size_t) offsetlength);
    loc[offsetlength] = '\0';
    (*i)++;//skip over '|'
}

int main(int argc, char** argv) {
    printf("Starting main\n");
    printf("SIZE OF TYPE %d \n", sizeof(TYPE));
    struct timespec alpha, indone, omega, fpga_start;

    //load input data
    char *in_file = "./include/gemmNcubed/input_uin32.data";
    char *check_file = "./include/gemmNcubed/check_ui32.data";
    int in_fd;
    void *data = malloc(INPUT_SIZE);
    in_fd = open(in_file, O_RDONLY);
    assert(in_fd >0 && "Couldn't open input data file");
    printf("BEFORE input to data\n");
    input_to_data(in_fd, data);
    printf("AFTER input to data\n");
    struct bench_args_t *args = (struct bench_args_t *)data;
    //printf("first data: %u \n", args->m1[0]);
    //printf("second data: %u \n", args->m2[0]);


    TYPE m1[N] = {1,2,3,4,5,6, 7, 8, 9};
    TYPE m2[N] = {3,4,5, 6,7,8,9,10, 11};
 
    struct timespec start, end;
    long transfer_time, run_time, rocc_sending_time, get_response_time;
    //uint64_t cmdAddr = static_balloc(NCORE * 6 + 1, 8 * sizeof(uint32_t));

    //***** SETTING UP REDUCER INPUT ADDRS
    int64_t m1InAddr = static_balloc(N,  sizeof(TYPE));
    int64_t m2InAddr = static_balloc(N, sizeof(TYPE));
    int64_t multOutAddr = static_balloc(N, sizeof(TYPE));
    
    simif_f1_t* handle = new simif_f1_t(0);

    
    //***** START REDUCER ON FPGA
    getcl(fpga_start);
    getcl(start);
    printf("Transferring array to FPGA...\n");
    transfer_chunk_to_fpga(handle, (uint32_t*)m1, m1InAddr, N * sizeof(TYPE));
    transfer_chunk_to_fpga(handle, (uint32_t*)m2, m2InAddr, N * sizeof(TYPE));
    //transfer_chunk_to_fpga(handle, (uint32_t*)args->m1, m1InAddr, N * sizeof(TYPE));
    //transfer_chunk_to_fpga(handle, (uint32_t*)args->m2, m2InAddr, N * sizeof(TYPE));
    printf("\n");
    fsync(handle->get_write_fd());
    getcl(end);
    printf("Data transfer for gemm ncubed took: ");
    transfer_time = get_elapsed(&start, &end);
    printf("Waiting for response from gemm ncubed...\n"); 

    
    //This is currently just running on core 0
    runGemmNcubed(handle, 0, m1InAddr, m2InAddr, multOutAddr);

    getcl(start);
    uint res_count = 0;
    std::pair<uint32_t, uint32_t> resp = get_id_retval(handle);
    cout << "ID : " << resp.first << " ERROR : " << resp.second << "\n";
    getcl(end);
    get_response_time = get_elapsed(&start, &end);
    run_time = get_elapsed(&fpga_start, &end);
    printf("Reducer took: ");
    printf("Total Data Transfer Time: %ld\n", transfer_time);
    cout << "Rocc_sending_time time: " << rocc_sending_time << endl;
    cout << "Get response_time time: " << get_response_time << endl;
    printf("Total Run Time: %ld\n", run_time);

    //TYPE* m1Actual = (TYPE*)malloc(N * sizeof(TYPE));
    //TYPE* m2Actual = (TYPE*)malloc(N * sizeof(TYPE));
    TYPE* outActual = (TYPE*)malloc(N * sizeof(TYPE));
    
    //get data back 
    printf("Getting data from fpga\n");
    //get_chunk_from_fpga(handle, (uint32_t*)m1Actual, m1InAddr, N * sizeof(TYPE));
    //get_chunk_from_fpga(handle, (uint32_t*)m2Actual, m2InAddr, N * sizeof(TYPE));
    get_chunk_from_fpga(handle, (uint32_t*)outActual, multOutAddr, N * sizeof(TYPE));
    fsync(handle->get_read_fd());
    
    //check data with output
    int check_fd;
    void *ref = malloc(INPUT_SIZE);
    check_fd = open( check_file, O_RDONLY );
    output_to_data(check_fd, ref);
    if(check_data(outActual, ref)){
      printf("SUCCESS\n");
    }

    //printf("M1 Actual is %u \n", m1Actual[1]);
    //printf("M2 Actual is %u \n", m2Actual[1]);
    for (uint32_t i = 0; i < N; i++){
      printf("Output Actual is %u \n", outActual[i]);
    }
    //printf("Output Actual is %u \n", outActual[0]);
    //printf("Output Actual is %u \n", outActual[1]);

    free(data);
    free(ref);
    //free(m1Actual);
    //free(m2Actual);
    free(outActual);
    fflush(stdout);
    delete handle;
    return 1;
}

static long get_elapsed(struct timespec* start, struct timespec* end) {
    long sec_diff, nsec_diff;
    if (end->tv_sec >= start->tv_sec) {
        sec_diff = end->tv_sec - start->tv_sec;
        if (end->tv_nsec >= start->tv_nsec) {
            nsec_diff = end->tv_nsec - start->tv_nsec;
            return sec_diff * 1000000000L + nsec_diff;
        } else {
            nsec_diff = start->tv_nsec - end->tv_nsec;
            return sec_diff * 1000000000L - nsec_diff;
        }
    } else {
        return 0;
    }
}
