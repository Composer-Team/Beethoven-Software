
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include "../include/csv.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#include "../include/fpga_transfer.h"

using namespace std;

int main(int argc, char **argv) {
    simif_f1_t* mysim = new simif_f1_t();

    int inSize = 20;
    uint64_t *inputs = (uint64_t*)malloc(sizeof(uint64_t)*inSize);
    for (int i = 0 ; i < inSize ; i++) {
        inputs[i] = (uint64_t) i;
        printf("inputs[%d] = %zu\n", i, inputs[i]);
    }

    uintptr_t inputAddr = 0;
    uintptr_t outputAddr = 1024;
    transfer_chunk_to_fpga_ocl(mysim, (uint32_t*)inputs, inputAddr, inSize * sizeof(uint64_t));
    
    example_set_input_addr(mysim, (void*)inputAddr);
    example_set_output_addr(mysim, (void*)outputAddr);

    example_start(mysim, 0, inSize);
    uint64_t unit_id = get_rocc_resp(mysim);
    printf("unit id %zu finished\n", unit_id);

    uint64_t *outputs = (uint64_t*)malloc(sizeof(uint64_t)*inSize);
    for (int i = 0 ; i < inSize ; i++) {
        outputs[i] = 0;
    } 
    get_chunk_from_fpga_ocl(mysim, (uint32_t*)outputs, outputAddr, inSize * sizeof(uint64_t));
    for (int i = 0 ; i < inSize ; i++) {
        printf("outputs[%d] = %zu\n", i, outputs[i]);
    }     
    delete mysim;
    return 1;
}
