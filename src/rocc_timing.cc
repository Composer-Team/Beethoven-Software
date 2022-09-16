#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>
#include <vector>
#include "../include/butil.h"

#include "../include/fpga_transfer.h"

using namespace std;

/** In bytes*/
simif_f1_t* handle;

void handle_signal(int signal);

void run(uint64_t num) {
    for (int i = 0; i < 8; i++) {
        bqsr_masterbuffer_set_addr(handle, 0, (void*) NULL, 0);
    }
    struct timespec s, e;
    getcl(s);
    for (uint64_t i = 0; i < num; i++) {
        encode_cmd_buf_xs(handle, CUSTOM_3, BQSR_MASTERBUFFER_BASE, 0, 0, OPCODE_MB, 0, false, false,
                          true); // return to host
        get_rocc_resp(handle);
    }
    getcl(e);
    printf("%lld,%lld\n", num, get_elapsed(s, e));
}

int main(int argc, char** argv) {

    handle = new simif_f1_t(0);

    struct sigaction sa;
    sa.sa_handler = &handle_signal;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT"); // Should not happen
    }

    for (uint64_t i = 1; i < argc; i++) {
        run(stoll(argv[i]));
    }


    delete handle;
    return 0;
}

void handle_signal(int signal) {
    switch (signal) {
        case SIGINT:
            delete handle;
            exit(1);
        default:
            return;
    }
}
