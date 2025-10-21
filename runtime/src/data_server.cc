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

#if AWS
uint8_t *devmem_aws_dram;
#endif

data_server_file *dsf;

[[noreturn]] static void *data_server_f(void *) {
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Starting data server thread" << std::endl;
  }
  int fd_beethoven = shm_open(data_server_file_name().c_str(), O_CREAT | O_RDWR, file_access_flags);
  if (fd_beethoven < 0) {
    std::cerr << "Failed to open data_server file: '" << data_server_file_name << "'" << std::endl;
    throw std::exception();
  }
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Opened shared memory: " << data_server_file_name() << std::endl;
  }

  struct stat shm_stats{};
  fstat(fd_beethoven, &shm_stats);
  LOG(std::cerr << shm_stats.st_size << std::endl;
      std::cerr << sizeof(data_server_file) << std::endl);
  if (shm_stats.st_size < sizeof(data_server_file)) {
    if (runtime_verbose) {
      std::cout << "[DATA_SERVER] Truncating shared memory to " << sizeof(data_server_file) << " bytes" << std::endl;
    }
    int tr_rc = ftruncate(fd_beethoven, sizeof(data_server_file));
    if (tr_rc) {
      std::cerr << "Failed to truncate data_server file" << std::endl;
      throw std::exception();
    }
  }

  auto &addr = *(data_server_file *) mmap(nullptr, sizeof(data_server_file), file_access_prots,
                                          MAP_SHARED, fd_beethoven, 0);
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Memory mapped successfully" << std::endl;
  }

#ifdef BEETHOVEN_USE_CUSTOM_ALLOC
  LOG(std::cerr << "Constructing allocator" << std::endl);
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Using custom allocator (size: " << ALLOCATOR_SIZE_BYTES << " bytes)" << std::endl;
  }
  auto allocator = new device_allocator<ALLOCATOR_SIZE_BYTES>();
  LOG(std::cerr << "Allocator constructed - data server ready" << std::endl);
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Allocator constructed" << std::endl;
  }
#endif
  data_server_file::init(addr);
  LOG(std::cerr << "Data server file constructed" << std::endl);
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Data server file initialized" << std::endl;
  }

#if defined(FPGA) && AWS
  int dmfd = open("/dev/mem", O_RDWR | O_SYNC);
  devmem_aws_dram = (uint8_t*)mmap(nullptr,
		  1L << 34,
		  PROT_READ | PROT_WRITE,
		  MAP_SHARED | MAP_FILE,
		  dmfd, 
		  0x52000000000ULL);
  if (devmem_aws_dram == MAP_FAILED) {
    printf("%s\n", strerror(errno));
    exit(0);
  }
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Running FPGA MemCpy Sanity Checks..." << std::endl;
  }
  auto sanity_alloc = (uint8_t *) malloc(1024);
  auto sanity_int = (uint32_t *) sanity_alloc;
  unsigned long sanity_address = 0xDEAD0000L;
  for (int i = 0; i < 1024 / 4; ++i) {
    sanity_int[i] = 0xCAFEBEEF;
  }
  if (runtime_verbose) {
    std::cout << "[DATA_SERVER] Trying to write 1024B to FPGA" << std::endl;
  }

  memcpy(devmem_aws_dram + sanity_address, sanity_int, 1024);
  memset(sanity_alloc, 0, 1024);
  memcpy(sanity_int, devmem_aws_dram + sanity_address, 1024);
  bool success = true;
  for (int i = 0; i < 1024 / 4; ++i) {
    if (sanity_int[i] != 0xCAFEBEEF) {
      success = false;
    }
  }

  if (!success) {
    std::cerr << "[DATA_SERVER] While the DMA read operation succeeded, the data we read back was faulty (not 0xCAFEBEEF)." << std::endl;
    throw std::exception();
  } else {
    if (runtime_verbose) {
      std::cout << "[DATA_SERVER] DMA data verification passed (3/3)" << std::endl;
    }
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
    if (runtime_verbose) {
      std::cout << "[DATA_SERVER] Received operation request" << std::endl;
    }
    switch (addr.operation) {
      case data_server_op::ALLOC: {
#if defined(FPGA) && defined(Kria)
        fprintf(stderr, "In Embedded FPGA runtime, client is attempting to allocate memory from"
                        "server. Allocations should only happen locally except for discrete boards.");
        fflush(stderr);
        break;
#endif
        if (runtime_verbose) {
          std::cout << "[DATA_SERVER] ALLOC: Allocating " << addr.op_argument << " bytes" << std::endl;
        }
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
        if (runtime_verbose) {
          std::cout << "[DATA_SERVER] ALLOC: Allocated " << nBytes << " bytes at host=" << naddr
                    << ", FPGA addr=0x" << std::hex << fpga_addr << std::dec << std::endl;
        }
#else
        auto fpga_addr = (uint64_t) naddr;
        at.add_mapping(fpga_addr, addr.op_argument, naddr);
        addr.op_argument = fpga_addr;
        if (runtime_verbose) {
          std::cout << "[DATA_SERVER] ALLOC: Allocated " << nBytes << " bytes at addr=0x"
                    << std::hex << fpga_addr << std::dec << std::endl;
        }
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
        if (runtime_verbose) {
          std::cout << "[DATA_SERVER] FREE: Freeing " << at.get_mapping(addr.op_argument).second
                    << " bytes at FPGA addr=0x" << std::hex << addr.op_argument << std::dec << std::endl;
        }
        munmap(at.get_mapping(addr.op_argument).first, at.get_mapping(addr.op_argument).second);
        at.remove_mapping(addr.op_argument);
        break;
      case data_server_op::MOVE_FROM_FPGA: {
        if (runtime_verbose) {
          std::cout << "[DATA_SERVER] MOVE_FROM_FPGA: Copying " << addr.op3_argument
                    << " bytes from FPGA addr=0x" << std::hex << addr.op2_argument << std::dec << std::endl;
        }
        // std::cerr << at.get_mapping(addr.op2_argument).first << std::endl;
#if defined(SIM) && defined(BEETHOVEN_HAS_DMA)
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
#elif defined(SIM)
#elif defined(FPGA) && AWS
        auto shaddr = at.translate(addr.op2_argument);
	auto len = addr.op3_argument;
	auto fpga_addr = addr.op2_argument;
        memcpy(shaddr, devmem_aws_dram + fpga_addr, len);
#elif defined(Kria)
	// do nothing
#else
#error "All cases not covered"
#endif
        break;
      }
      case data_server_op::MOVE_TO_FPGA: {
        if (runtime_verbose) {
          std::cout << "[DATA_SERVER] MOVE_TO_FPGA: Copying " << addr.op3_argument
                    << " bytes to FPGA addr=0x" << std::hex << addr.op_argument << std::dec << std::endl;
        }
#if defined(FPGA) && AWS
	auto fpga_addr = addr.op_argument;
        auto shaddr = at.translate(fpga_addr);
	auto len = addr.op3_argument;
        memcpy(devmem_aws_dram + fpga_addr, shaddr, len);
#elif Kria
	// do nothing
#elif SIM && defined(BEETHOVEN_HAS_DMA)
        auto amt_left = addr.op3_argument;
        auto ptr1 = (unsigned char *) at.translate(addr.op_argument);
        auto ptr2 = addr.op_argument;
        if (amt_left % 64 != 0) {
          if (runtime_verbose) {
            std::cout << "[DATA_SERVER] Warning: Data not aligned to 64-byte boundary" << std::endl;
          }
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
#endif
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
