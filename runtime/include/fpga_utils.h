//
// Created by Chris Kjellqvist on 10/29/22.
//

#ifndef BEETHOVEN_VERILATOR_FPGA_UTILS_H
#define BEETHOVEN_VERILATOR_FPGA_UTILS_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif
extern pthread_mutex_t main_lock;
#ifdef __cplusplus
};
#endif

#define AWS (defined(F1) || defined(F2))

#if AWS || defined(Kria)
extern pthread_mutex_t bus_lock;
#endif

#endif //BEETHOVEN_VERILATOR_FPGA_UTILS_H
