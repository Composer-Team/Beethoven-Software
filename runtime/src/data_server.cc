//
// Created by Chris Kjellqvist on 9/27/22.
//

#include <algorithm>
#ifdef USE_VCS
#include <vpi_user.h>
#endif
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include "util.h"
#include <beethoven/verilator_server.h>
#include <beethoven_hardware.h>

#include "../include/data_server.h"

#ifdef SIM
#ifdef VERILATOR
#include "sim/verilator.h"
#else
#include <vpi_user.h>
#endif
#endif

#include <fcntl.h>

#ifdef SIM
#ifdef BEETHOVEN_HAS_DMA
pthread_mutex_t dma_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dma_wait_lock = PTHREAD_MUTEX_INITIALIZER;
bool dma_in_progress = false;
uint64_t dma_fpga_addr;
bool dma_valid = false;
unsigned char *dma_ptr;
size_t dma_len;
bool dma_write;

#endif
#endif

#ifdef FPGA

#include "cmd_server.h"
#include "fpga_utils.h"
#include "mmio.h"

#endif

#ifdef Kria
#include <unistd.h>
#endif

#include <beethoven_hardware.h>

using namespace beethoven;

address_translator at;

#if defined(Kria)
static std::vector<uint16_t> available_ids;
#endif

#ifdef BEETHOVEN_USE_CUSTOM_ALLOC
#include "beethoven/allocator/device_allocator.h"
#endif




data_server_file *dsf;

[[noreturn]] static void *data_server_f(void *) {
  int fd_beethoven = shm_open(data_server_file_name().c_str(), O_CREAT | O_RDWR, file_access_flags);
  if (fd_beethoven < 0) {
    std::cerr << "Failed to open data_server file: '" << data_server_file_name << "'" << std::endl;
    throw std::exception();
  }

  struct stat shm_stats{};
  fstat(fd_beethoven, &shm_stats);
  LOG(std::cerr << shm_stats.st_size << std::endl;
      std::cerr << sizeof(data_server_file) << std::endl);
  if (shm_stats.st_size < sizeof(data_server_file)) {
    int tr_rc = ftruncate(fd_beethoven, sizeof(data_server_file));
    if (tr_rc) {
      std::cerr << "Failed to truncate data_server file" << std::endl;
      throw std::exception();
    }
  }

  auto &addr = *(data_server_file *) mmap(nullptr, sizeof(data_server_file), file_access_prots,
                                          MAP_SHARED, fd_beethoven, 0);

#ifdef BEETHOVEN_USE_CUSTOM_ALLOC
  LOG(std::cerr << "Constructing allocator" << std::endl);
  auto allocator = new device_allocator<ALLOCATOR_SIZE_BYTES>();
  LOG(std::cerr << "Allocator constructed - data server ready" << std::endl);
#endif
  data_server_file::init(addr);
  LOG(std::cerr << "Data server file constructed" << std::endl);

#if defined(FPGA) && AWS
  std::cerr << "Running FPGA MemCpy Sanity Checks..." << std::endl;
  auto sanity_alloc = (uint8_t *) malloc(1024);
  auto sanity_int = (uint32_t *) sanity_alloc;
  unsigned long sanity_address = 0xDEAD0000L;
  for (int i = 0; i < 1024 / 4; ++i) {
    sanity_int[i] = 0xCAFEBEEF;
  }
  std::cerr << "Trying to write 1024B to FPGA." << std::endl;

  int sanity_rc = wrapper_fpga_dma_burst_write(xdma_write_fd, sanity_alloc, 1024, sanity_address);
  if (sanity_rc) {
    std::cerr << "Failed to DMA write to FPGA. Error code: " << sanity_rc << std::endl;
    throw std::exception();
  } else {
    std::cerr << "Success 1/3" << std::endl;
  }
  memset(sanity_alloc, 0, 1024);
  sanity_rc = wrapper_fpga_dma_burst_read(xdma_read_fd, sanity_alloc, 1024, sanity_address);
  if (sanity_rc) {
    std::cerr << "Failed to DMA read from FPGA. Error code: " << sanity_rc << std::endl;
    throw std::exception();
  } else {
    std::cerr << "Success 2/3" << std::endl;
  }

  for (int i = 0; i < 1024 / 4; ++i) {
    if (sanity_int[i] != 0xCAFEBEEF) {
      sanity_rc = 1;
    }
  }

  if (sanity_rc) {
    std::cerr << "While the DMA read operation succeeded, the data we read back was faulty (not 0xCAFEBEEF)." << std::endl;
    throw std::exception();
  } else {
    std::cerr << "Success 3/3" << std::endl;
  }
#endif

  srand(time(nullptr));// NOLINT(cert-msc51-cpp)
  pthread_mutex_lock(&addr.server_mut);
  pthread_mutex_lock(&addr.server_mut);
#if defined(SIM) && defined(BEETHOVEN_HAS_DMA)
  pthread_mutex_lock(&dma_wait_lock);
#endif
  while (true) {
    //    printf("data server got cmd\n"); fflush(stdout);
    // get file name, descriptor, expand the file, and map it to address space
    switch (addr.operation) {
      case data_server_op::ALLOC: {
#if defined(FPGA) && defined(Kria)
        fprintf(stderr, "In Embedded FPGA runtime, client is attempting to allocate memory from"
                        "server. Allocations should only happen locally except for discrete boards.");
        fflush(stderr);
        break;
#endif
        auto fname = "/beethoven_file_" + std::to_string(rand());// NOLINT(cert-msc50-cpp)
        int nfd = shm_open(fname.c_str(), O_CREAT | O_RDWR, file_access_flags);
        if (nfd < 0) {
          std::cerr << "Failed to open shared memory segment: " << std::string(strerror(errno)) << std::endl;
          throw std::exception();
        }
        int rc = ftruncate(nfd, (off_t) addr.op_argument);
        if (rc) {
          std::cerr << "Failed to truncate!" << std::endl;
          //          printf("Failed to truncate! - %d, %d, %llu\t %s\n", rc, nfd, (off_t) addr.op_argument, strerror(errno));
          throw std::exception();
        }
        void *naddr = mmap(nullptr, addr.op_argument, file_access_prots, MAP_SHARED, nfd, 0);


        if (naddr == nullptr) {
          std::cerr << "Failed to mmap address: " << std::string(strerror(errno)) << std::endl;
          throw std::exception();
        }
        memset(naddr, 0, addr.op_argument);
        auto nBytes = addr.op_argument;
#ifdef Kria
        unsigned int cacheLineSz = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
        char *ptr = (char *) naddr;
        for (uint64_t i = 0; i < addr.op_argument / cacheLineSz; ++i) {
          asm volatile("DC CIVAC, %0" ::"r"(ptr)
                       : "memory");
          ptr += cacheLineSz;
        }
#endif
        //write response
        // copy file name to response field
        strcpy(addr.fname, fname.c_str());
        // allocate memory
#ifdef BEETHOVEN_USE_CUSTOM_ALLOC
        auto fpga_addr = allocator->malloc(addr.op_argument);
        at.add_mapping(fpga_addr, addr.op_argument, naddr);
        // return fpga address
        addr.op_argument = fpga_addr;
        LOG(printf("Allocated %llu bytes at %p. FPGA addr %llx\n", nBytes, naddr, fpga_addr));
#else
        auto fpga_addr = (uint64_t) naddr;
        at.add_mapping(fpga_addr, addr.op_argument, naddr);
        addr.op_argument = fpga_addr;
#endif
        // add mapping in server
        break;
      }
      case data_server_op::FREE:
#ifdef BEETHOVEN_USE_CUSTOM_ALLOC
        allocator->free(addr.op_argument);
#endif
        LOG(printf("Freeing %llu bytes at %p\n", at.get_mapping(addr.op_argument).second, at.get_mapping(addr.op_argument).first);
            fflush(stdout));
        munmap(at.get_mapping(addr.op_argument).first, at.get_mapping(addr.op_argument).second);
        at.remove_mapping(addr.op_argument);
        break;
      case data_server_op::MOVE_FROM_FPGA: {
        // std::cerr << at.get_mapping(addr.op2_argument).first << std::endl;
#ifdef SIM
#ifdef BEETHOVEN_HAS_DMA
        auto ptr1 = (unsigned char *) at.translate(addr.op2_argument);
        auto ptr2 = addr.op2_argument;
        auto amt_left = addr.op3_argument;
        while (amt_left > 0) {
          auto n_beats_here = std::max(uint64_t(1), rand() % std::min(uint64_t(64), amt_left >> 6));
          pthread_mutex_lock(&dma_lock);
          dma_len = 64 * n_beats_here;
          amt_left -= n_beats_here * 64;
          dma_ptr = ptr1;
          dma_fpga_addr = ptr2;
          dma_valid = true;
          dma_write = false;
          dma_in_progress = false;
          pthread_mutex_unlock(&dma_lock);
          pthread_mutex_lock(&dma_wait_lock);
          ptr1 += 64 * n_beats_here;
          ptr2 += 64 * n_beats_here;
        }
#endif
#elif defined(FPGA) // end SIM
#ifndef Kria
        auto shaddr = at.translate(addr.op2_argument);
        //        std::cout << "from fpga addr: " << addr.op2_argument << std::endl;
        auto *mem = (uint8_t *) malloc(addr.op3_argument);
        int rc = wrapper_fpga_dma_burst_read(xdma_read_fd, (uint8_t *) mem, addr.op3_argument, addr.op2_argument);
        memcpy(shaddr, mem, addr.op3_argument);
        free(mem);
        //        for (int i = 0; i < addr.op3_argument / sizeof(int); ++i)
        //          printf("%d ", ((int *) shaddr)[i]);
        //        fflush(stdout);
        if (rc) {
          fprintf(stderr, "Something failed inside MOVE_FROM_FPGA - %d %p %ld %lx\n", xdma_read_fd, shaddr,
                  addr.op3_argument, addr.op2_argument);
          exit(1);
        }
#else // end ndef Kria
        fprintf(stderr, "Kria backend attempting to do unsupported op in data server\n");
#endif
#else // end SIM/FPGA
#error "All cases not covered"
#endif
        break;
      }
      case data_server_op::MOVE_TO_FPGA: {
#ifdef FPGA
#ifndef Kria // implied discrete
        auto shaddr = at.translate(addr.op_argument);
        auto *mem = (uint8_t *) malloc(addr.op3_argument);
        memcpy(mem, shaddr, addr.op3_argument);
        int rc = wrapper_fpga_dma_burst_write(xdma_write_fd, (uint8_t *) shaddr, addr.op3_argument, addr.op_argument);
        if (rc) {
          fprintf(stderr, "Something failed inside MOVE_TO_FPGA - %d %p %ld %lx\n", xdma_write_fd, shaddr,
                  addr.op3_argument, addr.op2_argument);
          exit(1);
        }
#else
        fprintf(stderr, "Kria backend attempting to do unsupported op in data server\n");
#endif // end Kria
#else // end FPGA, else SIM
#if defined(BEETHOVEN_HAS_DMA)
        auto amt_left = addr.op3_argument;
        auto ptr1 = (unsigned char *) at.translate(addr.op_argument);
        auto ptr2 = addr.op_argument;
        if (amt_left % 64 != 0) {
          printf("NOT ALIGNED OOF DATA\n");
        }
        while (amt_left > 0) {
          pthread_mutex_lock(&dma_lock);
          auto n_beats_here = std::max(uint64_t(1), rand() % std::min(uint64_t(64), amt_left >> 6));
          dma_len = 64 * n_beats_here;
//          printf("REMAINING %d - %x\n", amt_left, dma_len);
          dma_ptr = ptr1;
          amt_left -= n_beats_here * 64;
          dma_fpga_addr = ptr2;
          dma_valid = true;
          dma_write = true;
          dma_in_progress = false;
          pthread_mutex_unlock(&dma_lock);
          pthread_mutex_lock(&dma_wait_lock);
          ptr1 += 64 * n_beats_here;
          ptr2 += 64 * n_beats_here;
        }
#endif // end DMA
#endif // end SIM
        break;
      }
    }
    // un-lock client to read response
    pthread_mutex_unlock(&addr.data_cmd_recieve_resp_lock);
    // re-lock self to stall
    pthread_mutex_lock(&addr.server_mut);
  }
}

void data_server::start() {
  pthread_t thread;
  pthread_create(&thread, nullptr, data_server_f, nullptr);
}

void *address_translator::translate(uint64_t fp_addr) const {
  auto it = mappings.begin();
  while (it != mappings.end()) {
    if (it->fpga_addr <= fp_addr and it->fpga_addr + it->mapping_length > fp_addr) {
      break;
    }
    it++;
  }
  if (it == mappings.end()) {
    std::cerr << "BAD ADDRESS IN TRANSLATION FROM FPGA -> CPU: " << std::hex << fp_addr << ". You might be running outside of your allocated segment... " << std::endl;
    std::cerr << "Existing Mappings:" << std::endl;
    for (auto q: mappings) {
      std::cerr << q.fpga_addr << "\t" << q.mapping_length << std::endl;
    }
#if defined(SIM) && defined(VERILATOR)
    tfp->close();
#endif
#if defined(SIM) && !defined(VERILATOR)
    vpi_control(vpiFinish);
    return nullptr;
#else
    throw std::exception();
#endif
  }
  if (it->fpga_addr + it->mapping_length <= fp_addr) {
    std::cerr << "ADDRESS IS OUT OF BOUNDS FROM FPGA -> CPU\n"
              << std::endl;
#if defined(SIM) && defined(USE_VCS)
      vpi_control(vpiFinish);
#endif
    throw std::exception();
  }
  return (char *) it->cpu_addr + (fp_addr - it->fpga_addr);
}

void address_translator::add_mapping(uint64_t fpga_addr, uint64_t mapping_length, void *cpu_addr) {
  mappings.emplace(fpga_addr, cpu_addr, mapping_length);
}

void address_translator::remove_mapping(uint64_t fpga_addr) {
  addr_pair a(fpga_addr, nullptr, 0);
  auto it = mappings.find(a);
  if (it == mappings.end()) {
    std::cerr << "ERROR - could not remove mapping in data server because could not find address...\n"
              << std::endl;
    throw std::exception();
  }
  mappings.erase(it);
}

std::pair<void *, uint64_t> address_translator::get_mapping(uint64_t fpga_addr) const {
  auto it = mappings.begin();
  while (it != mappings.end()) {
    if (it->fpga_addr == fpga_addr) {
      return std::make_pair(it->cpu_addr, it->mapping_length);
    }
    it++;
  }
  std::cerr << "Mapping not found!" << std::endl;
  throw std::exception();
}

data_server::~data_server() {
  munmap(&dsf, sizeof(data_server_file));
  shm_unlink(data_server_file_name().c_str());
}
