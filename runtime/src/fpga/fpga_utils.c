//
// Created by Chris Kjellqvist on 10/29/22.
//
#include "fpga_utils.h"

#if AWS || defined(ZYNQ)
#include <pthread.h>
pthread_mutex_t bus_lock;
#endif
