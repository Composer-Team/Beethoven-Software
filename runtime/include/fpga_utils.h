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

#if defined(F1) || defined(F2)
#define AWS 1
#if defined(F1)
#define USE_XDMA 1
#else
#define USE_XDMA 0
#endif
#else
#define AWS 0
#define USE_XDMA 0
#endif

#if AWS || defined(Kria)
extern pthread_mutex_t bus_lock;
#endif
#if AWS
#if defined(VSIM)
#include "fpga_pci_sv.h"
#else
#include "fpga_mgmt.h"
#include "fpga_pci.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


extern int pci_bar_handle;
extern int xdma_write_fd;
extern int xdma_read_fd;


void check_rc(int rc, const char *message);

void fpga_setup(int id);

void fpga_shutdown();

int wrapper_fpga_dma_burst_write(int fd, uint8_t *buffer, size_t xfer_sz,
                                 size_t address);

int wrapper_fpga_dma_burst_read(int fd, uint8_t *buffer, size_t xfer_sz,
                                size_t address);

#ifdef __cplusplus
}
#endif

#endif

#endif //BEETHOVEN_VERILATOR_FPGA_UTILS_H
