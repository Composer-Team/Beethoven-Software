//
// Created by Chris Kjellqvist on 10/29/22.
//
#include "fpga_utils.h"

#if AWS || defined(Kria)
#include <pthread.h>
pthread_mutex_t bus_lock;
#endif

#if AWS
#include "fpga_aws_utils.c"
#endif
