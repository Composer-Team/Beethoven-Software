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
#include <unistd.h>
#include <bitset>
#include "../include/fpga_transfer.h"
#include "../include/butil.h"
#include "../include/memrocc.h"
//this is specific to machSuite so that it works with the C files obtained from MachSuite github
using namespace std;


#define MAXCHAR 1000

static long get_elapsed(struct timespec* start, struct timespec* end);
void help_text() {
    printf("Wrong input\n");
}

const int N = 3;
// Parameter mapping
void runGemmNcubed(fpga_handle_t *handle, int core,
                      uint64_t inputAddrs1, uint64_t inputAddrs2, uint64_t outputAddr) {
    printf("Setting up gemm ncubed unit...\n");
    
    /*
     * Functions are defined in ./include/rocc.h
     * TODO: set addr tells fpga where the address for the inputs and outputs are
     * always set the read channels before write channels 
     * gemm_set_len is an example of a function where you can send a length 
     * gemm_start tells the fpga to start the simulator 
    */
    gemm_set_addr(handle, 0, (void*) inputAddrs1, core); //read channel 0
    gemm_set_addr(handle, 1, (void*) inputAddrs2, core); //read channel 1
    gemm_set_addr(handle, 2, (void*) outputAddr, core); //write channel 0
    //gemm_set_len(handle, row_size, 1, core);
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
    struct timespec alpha, indone, omega, fpga_start;

    //TODO: load input data
    TYPE m1[N] = {1,2,3,4,5,6, 7, 8, 9};
    TYPE m2[N] = {3,4,5, 6,7,8,9,10, 11};
 
    struct timespec start, end;
    long transfer_time, run_time, rocc_sending_time, get_response_time;
   
    //***** SETTING UP INPUT ADDRS
    //TODO: change name and size - similar to malloc
    int64_t m1InAddr = static_balloc(N,  sizeof(TYPE));
    int64_t m2InAddr = static_balloc(N, sizeof(TYPE));
    int64_t multOutAddr = static_balloc(N, sizeof(TYPE));
    
    auto handle = new fpga_handle_real_t(0); //simulation handle

    
    //***** START ON FPGA and collect timing info
    getcl(fpga_start);
    getcl(start);
    printf("Transferring array to FPGA...\n");
    //TODO: change name and size to correpond to the input addr above
    //sends input to fpga
    transfer_chunk_to_fpga(handle, (uint32_t*)m1, m1InAddr, N * sizeof(int));
    transfer_chunk_to_fpga(handle, (uint32_t*)m2, m2InAddr, N * sizeof(int));
    fsync(handle->get_write_fd());
    getcl(end);
    printf("Data transfer for gemm ncubed took: ");
    transfer_time = get_elapsed(&start, &end);
    printf("Waiting for response from gemm ncubed...\n"); 

    
    //This is currently just running on core 0
    runGemmNcubed(handle, 0, m1InAddr, m2InAddr, multOutAddr);

    //calculate how to run module
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

    //TODO: change name and size
    //create pointer to an array to receive output from fpga
    TYPE* outActual = (TYPE*)malloc(N * sizeof(TYPE));
    
    //get data back 
    printf("Getting data from fpga\n");
    //TODO: change name and size
    get_chunk_from_fpga(handle, (uint32_t*)outActual, multOutAddr, N * sizeof(TYPE));
    fsync(handle->get_read_fd());
    
    //TODO: free ptrs
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
