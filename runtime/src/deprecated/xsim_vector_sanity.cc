//
// Created by Chris Kjellqvist on 11/17/22.
//
#include "beethoven_allocator_declaration.h"

#include <beethoven/rocc_cmd.h>

extern "C" {


#ifdef FPGA
#include <fpga_pci.h>
#include <fpga_mgmt.h>
#else
#include "fpga_pci_sv.h"
#include "sh_dpi_tasks.h"
pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;
static uint8_t *send_rdbuf_to_c_read_buffer = nullptr;
static size_t send_rdbuf_to_c_buffer_size = 0;

void setup_send_rdbuf_to_c(uint8_t *read_buffer, size_t buffer_size) {
  send_rdbuf_to_c_read_buffer = read_buffer;
  send_rdbuf_to_c_buffer_size = buffer_size;
}

int send_rdbuf_to_c(char *rd_buf) {
  int i;

  /* For Questa simulator the first 8 bytes are not transmitted correctly, so
   * the buffer is transferred with 8 extra bytes and those bytes are removed
   * here. Made this default for all the simulators. */
  for (i = 0; i < send_rdbuf_to_c_buffer_size; ++i) {
    send_rdbuf_to_c_read_buffer[i] = rd_buf[i + 8];
  }

  /* end of line character is not transferered correctly. So assign that
   * here. */
  /*send_rdbuf_to_c_read_buffer[send_rdbuf_to_c_buffer_size - 1] = '\0';*/

  return 0;
}
static inline int do_dma_read(uint8_t *buffer, size_t size,
                              uint64_t address, int channel, int slot_id) {
  sv_fpga_start_cl_to_buffer(slot_id, channel, size, (uint64_t) buffer, address);
  return 0;
}

static inline int do_dma_write(uint8_t *buffer, size_t size,
                               uint64_t address, int channel, int slot_id) {
  sv_fpga_start_buffer_to_cl(slot_id, channel, size, (uint64_t) buffer, address);
  return 0;
}

#endif
};

void write_command(uint32_t *p) {
  for (int i = 0; i < 5; ++i) {
    uint32_t ready = 0;
    while (!ready) fpga_pci_peek(pci_bar_handle, CMD_READY, &ready);
    fpga_pci_poke(pci_bar_handle, CMD_BITS, p[i]);
    fpga_pci_poke(pci_bar_handle, CMD_VALID, 1);
  }
}

uint32_t *get_response() {
  auto *v = new uint32_t[3];
  for (int i = 0; i < 3; ++i) {
    uint32_t ready = 0;
    while (!ready) fpga_pci_peek(pci_bar_handle, RESP_VALID, &ready);
    fpga_pci_peek(pci_bar_handle, RESP_BITS, &ready);
    v[i] = ready;
    fpga_pci_poke(pci_bar_handle, RESP_READY, 1);
  }
  return v;
}

#ifdef FPGA
#define cosim_printf printf
int main()
#else
extern "C" void test_main_hook(int *rc)
#endif
{

  // read addr: 1ff000-1ff800
  //write addr: 1fe000-1fe800
  //Sending addresses           1ff000 and           1fe000 to fpga
  //Sent data
  //command in file is function: 0 system_id: 1 opcode: { rs1_num:   rs2_num:  xd:   rd: 0 xs1:   xs2:   core_id:   rs1: 2048 rs2: 2093056
  //detected addr command
  //addr_read: 1010007b	0	4001	0	1ff000
  //command in file is function: 0 system_id: 1 opcode: { rs1_num:   rs2_num:   xd:   rd: 0 xs1:   xs2:   core_id:   rs1: 2048 rs2: 2088960
  //detected addr command
  //addr_write: 1000007b	0	4000	0	1fe000
  //start_cmd: 1200407b	0	400	0	f

  cosim_printf("enter function\n");
  fflush(stdout);
  fpga_mgmt_init();
  int n = 1024;
  unsigned long buffer_size = sizeof(uint16_t) * n;
  auto *read_buffer = new uint16_t[n];
  for (int i = 0; i < n; ++i) {
    read_buffer[i] = i;
  }
  cosim_printf("initialized\n");
  fflush(stdout);
  // hopefully this makes the ddr start up and train...
  init_ddr();
  deselect_atg_hw();

  cosim_printf("FINished init ddr\n");

  cosim_printf("Trying to write to read buffer\n");
  do_dma_write((uint8_t*)read_buffer, buffer_size, 0x1ff000, 0, 0);

  uint32_t addr1[] = {0x1010007b, 0, 0x4001, 0x0, 0x1ff000};
  uint32_t addr2[] = {0x1000007b, 0, 0x4000, 0x0, 0x1fe000};
  uint32_t start[] = {0x1200407b, 0, 0x400, 0x0, 0xf};


#ifdef FPGA
  int rc = fpga_pci_attach(0, FPGA_APP_PF, APP_PF_BAR0, 0, &pci_bar_handle);
  if (rc) {
    printf("Fail\n");
    exit(1);
  }
#endif
  uint32_t ready;
  write_command(addr1);
  cosim_printf("Wrote a command\n");
  write_command(addr2);
  write_command(start);
  auto dat = get_response();

  auto *wrbuf = new uint16_t[n];
  do_dma_read((uint8_t*)wrbuf, buffer_size, 0x1fe000, 0, 0);

  for (int i = 0; i < n; ++i) {
    cosim_printf("Got %x, expected %x\n", wrbuf[i], i+15);
  }

  fpga_mgmt_close();
#ifndef FPGA
  *rc = 0;
#endif
}
