#ifndef COMPOSER_CONSTS_H
#define COMPOSER_CONSTS_H

#define ROCC_OP_ACCEL 0x7b
#define ROCC_OP_FLUSH 0xb

#define ROCC_FUNC_ADDR 0
#define ROCC_FUNC_START 1

#define RESP_BITS 0 //READONLY
#define RESP_VALID 1 //READONLY
#define RESP_READY 2 //WRITEONLY

#define CMD_BITS 3 //WRITEONLY
#define CMD_VALID 4 //WRITEONLY
#define CMD_READY 5 //READONLY

#define BITS_PER_CACHE_LINE 1024
#define BITS_PER_BYTE 8

#define DMA_BLOCK_SIZE 16777216

#define WRITE_BITS 6 //WRITEONLY
#define WRITE_VALID 7 //WRITEONLY
#define WRITE_READY 8 //READONLY

#define READ_ADDR_BITS 9 //WRITEONLY
#define READ_ADDR_VALID 10 //WRITEONLY
#define READ_ADDR_READY 11 //READONLY

#define READ_BITS 12 //READONLY
#define READ_VALID 13 //READONLY
#define READ_READY 14 //WRITEONLY

/**
 * RoCC commands have a destination register (rd) that is sent to Composer systems. Composer reserves a number of these
 * registers for AXI-Mem port statistics.
 *
 * The AXI Spec (https://developer.arm.com/documentation/102202/0300/Channel-signals) has 5 ports for memory \n
 * AW - Write address port \n
 * W - Write data port \n
 * B - Write response port \n
 * AR - Read address port \n
 * R - Read response port \n
 *
 * Composer counts the number of responses for each one of these ports for debugging purposes (presumably)
 * Composer also collects the number of cycles spent waiting for read responses(21) and for write responses(22)
 */
enum RD {
  /**
   * General purpose registers
   */
  R0 = 0,
  R1 = 1,
  R2 = 2,
  R3 = 3,
  R4 = 4,
  R5 = 5,
  R6 = 6,
  R7 = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,
  /**
   * Special Registers
   **/
  AddressReadCnt = 16,
  AddressWriteCnt = 17,
  ReadCnt = 18,
  WriteCnt = 19,
  WriteResponseCnt = 20,
  ReadWait = 21,
  WriteWait = 22,
  /**
   * More general purpose registers
   */
  R23,
  R24,
  R25,
  R26,
  R27,
  R28,
  R29,
  R30,
  R31
};

#endif
