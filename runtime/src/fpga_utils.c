//
// Created by Chris Kjellqvist on 10/29/22.
//
#include "fpga_utils.h"
#if AWS || defined(Kria)
#include <pthread.h>
pthread_mutex_t bus_lock;
#endif
#ifdef AWS
#ifndef VSIM
#include "fpga_dma.h"
#endif

#include <stdio.h>
#include <stdlib.h>

int pci_bar_handle;
int xdma_write_fd;
int xdma_read_fd;


void check_rc(int rc, const char *message) {
  if (rc) {
    fprintf(stderr, "Failure: '%s'\t%d\n", message, rc);
    exit(rc);
  }
}

void fpga_setup(int slot_id) {
  int rc = fpga_mgmt_init();
  check_rc(rc, "fpga_mgmt_init FAILED");
#if AWS
  /* check AFI status */
  struct fpga_mgmt_image_info info = {0};

  /* get local image description, contains status, vendor id, and device id. */
  rc = fpga_mgmt_describe_local_image(slot_id, &info, 0);
  check_rc(rc, "Unable to get AFI information from slot. Are you running as root?");

  /* check to see if the slot is ready */
  if (info.status != FPGA_STATUS_LOADED) {
    rc = 1;
    check_rc(rc, "AFI in Slot is not in READY state !");
  }

    /* get local image description, contains status, vendor id, and device id. */
    rc = fpga_mgmt_describe_local_image(slot_id, &info, 0);
    check_rc(rc, "Unable to get AFI information from slot");

  /* attach to BAR0 */
  pci_bar_handle = PCI_BAR_HANDLE_INIT;
  rc = fpga_pci_attach(slot_id, FPGA_APP_PF, APP_PF_BAR0, 0, &pci_bar_handle);
  check_rc(rc, "fpga_pci_attach FAILED");

#if USE_XDMA

  xdma_read_fd = fpga_dma_open_queue(FPGA_DMA_XDMA, slot_id, 0, true);
  if (xdma_read_fd < 0) {
    fprintf(stderr, "Error opening XDMA read fd\n");
    exit(1);
  }
  xdma_write_fd = fpga_dma_open_queue(FPGA_DMA_XDMA, slot_id, 0, false);
  if (xdma_write_fd < 0) {
    fprintf(stderr, "Error opening XDMA write fd\n");
    exit(1);
  }
#endif
#endif
}


void fpga_shutdown() {
  int rc = fpga_mgmt_close();
  // don't call check_rc because of fpga_shutdown call. do it manually:
  check_rc(rc, "Failure while detaching from the fpga");
}

int wrapper_fpga_dma_burst_write(int fd, uint8_t *buffer, size_t xfer_sz,
                                 size_t address) {
#if USE_XDMA
  return fpga_dma_burst_write(fd, buffer, xfer_sz, address);
#endif
  return 0;
}

int wrapper_fpga_dma_burst_read(int fd, uint8_t *buffer, size_t xfer_sz,
                                size_t address) {
#if USE_XDMA
  return fpga_dma_burst_read(fd, buffer, xfer_sz, address);
#else
  return 0;
#endif
}

#endif
