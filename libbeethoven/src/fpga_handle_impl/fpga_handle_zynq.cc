//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "beethoven/fpga_handle.h"
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "beethoven/allocator/alloc.h"
#include "beethoven/arm_cache.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-heap.h>
#include <pthread.h>

const unsigned zynq_huge_page_sizes[] = {1 << 21, 1 << 25, 1 << 30};
const unsigned zynq_huge_page_flags[] = {21 << MAP_HUGE_SHIFT, 25 << MAP_HUGE_SHIFT,
                                         30 << MAP_HUGE_SHIFT};
const unsigned zynq_n_page_sizes = sizeof(zynq_huge_page_sizes) / sizeof(zynq_huge_page_sizes[0]);


using namespace beethoven;


std::vector<fpga_handle_t *> beethoven::active_fpga_handles;

fpga_handle_t *beethoven::current_handle_context;

[[maybe_unused]] void beethoven::set_fpga_context(fpga_handle_t *handle) {
  current_handle_context = handle;
  if (std::find(active_fpga_handles.begin(), active_fpga_handles.end(), handle) == active_fpga_handles.end()) {
    std::cerr << "The provided handle appears to have not been properly constructed. Please use the provided"
                 " fpga_handle_t constructors." << std::endl;
    exit(1);
  }
}
/*
 * Copyright (c) 2019,
 * The University of California, Berkeley and Duke University.
 * All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "beethoven/runtime_ipc.h"
#include "beethoven/response_handle.h"
#include <unistd.h>
#ifndef ZYNQ
#include <pthread.h>
#endif

#include <fcntl.h>

using namespace beethoven;

// https://stackoverflow.com/questions/2440385/how-to-find-the-physical-address-of-a-variable-from-user-space-in-linux
#include "cstdio"
#include "cinttypes"

uint64_t vtop(unsigned long virt_addr) {
  FILE *pagemap;
  uint64_t paddr = 0;
  uint64_t offset = (virt_addr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
  uint64_t e;
  if ((pagemap = fopen("/proc/self/pagemap", "r"))) {
    if (lseek(fileno(pagemap), (long)offset, SEEK_SET) == offset) {
      if (fread(&e, sizeof(uint64_t), 1, pagemap)) {
        if (e & (1ULL << 63)) { // page present ?
          paddr = e & ((1ULL << 54) - 1); // pfn mask
          paddr = paddr * sysconf(_SC_PAGESIZE);
          // add offset within page
          paddr = paddr | (virt_addr & (sysconf(_SC_PAGESIZE) - 1));
        }
      }
    }
    fclose(pagemap);
  }
  return paddr;
}


static size_t page_align(size_t len) {
  const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  return (len + page - 1) & ~(page - 1);
}

static bool verify_contiguous_mapping(void *addr, size_t len) {
  const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  const uint64_t first = vtop(reinterpret_cast<unsigned long>(addr));
  if (first == 0) return false;
  for (size_t off = page; off < len; off += page) {
    const uint64_t phys = vtop(reinterpret_cast<unsigned long>(addr) + off);
    if (phys != first + off) return false;
  }
  return true;
}

static remote_ptr try_dma_heap_malloc(size_t len) {
  const size_t sz = page_align(len);
  int heap_fd = open("/dev/dma_heap/reserved", O_RDWR | O_CLOEXEC);
  if (heap_fd < 0) {
    throw std::runtime_error("reserved dma_heap unavailable: " + std::string(strerror(errno)));
  }

  dma_heap_allocation_data allocation{};
  allocation.len = sz;
  allocation.fd_flags = O_RDWR | O_CLOEXEC;
  allocation.heap_flags = 0;
  if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &allocation) < 0) {
    const std::string err = strerror(errno);
    close(heap_fd);
    throw std::runtime_error("reserved dma_heap alloc failed: " + err);
  }
  close(heap_fd);

  void *addr = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, allocation.fd, 0);
  if (addr == MAP_FAILED) {
    const std::string err = strerror(errno);
    close(allocation.fd);
    throw std::runtime_error("reserved dma_heap mmap failed: " + err);
  }

  // Fault every page in before consulting pagemap. Zeroing is acceptable for a
  // fresh bridge-owned buffer and avoids sparse, not-present page translations.
  std::memset(addr, 0, sz);
  if (!verify_contiguous_mapping(addr, sz)) {
    munmap(addr, sz);
    close(allocation.fd);
    throw std::runtime_error("reserved dma_heap returned a non-contiguous mapping");
  }

  const uint64_t phys = vtop(reinterpret_cast<unsigned long>(addr));
  close(allocation.fd);
  return remote_ptr(static_cast<intptr_t>(phys), addr, sz);
}

static int cacheLineSz;
static int logCacheLineSz;

fpga_handle_t::fpga_handle_t() {
  cacheLineSz = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  int t = cacheLineSz;
  logCacheLineSz = 0;
  while (t != 0) {
    logCacheLineSz++;
    t >>= 1;
  }
  // map in command server
  auto cmd_fname = cmd_server_file_name();
  csfd = shm_open(cmd_fname.c_str(), O_RDWR, file_access_flags);
  if (csfd == -1) {
    std::cerr << "Error opening file " << cmd_fname << " " << strerror(errno) << std::endl;
    exit(1);
  }
  cmd_server = (cmd_server_file *) mmap(nullptr, sizeof(cmd_server_file), file_access_prots, MAP_SHARED, csfd, 0);
  if (cmd_server == MAP_FAILED) {
    std::cerr << "Failed to map in cmd_server_file\t" << strerror(errno) << std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }

  cmd_server->quit = false;

  // map in data server
  auto dataserver_fname = data_server_file_name();
  dsfd = shm_open(dataserver_fname.c_str(), O_RDWR, file_access_flags);
  if (dsfd < 0) {
    std::cerr << "Error opening file " << dataserver_fname << std::endl;
    exit(1);
  }
  data_server = (data_server_file *) mmap(nullptr, sizeof(data_server_file), file_access_prots, MAP_SHARED, dsfd, 0);
  if (data_server == MAP_FAILED) {
    std::cerr << "Failed to map in data_server_file" << std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }
  active_fpga_handles.push_back(this);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"

#pragma clang diagnostic pop

fpga_handle_t::~fpga_handle_t() {
  munmap(cmd_server, sizeof(cmd_server_file));
  close(csfd);
  while (not device2virtual.empty()) {
    auto tup = device2virtual.begin();
    // TODO consider - is it proper to free it? fpga_handle_t has to be deconstructed but user may manually free segments
    //    in which case maybe the cleanest way for us to handle this is just implement unique_ptr
//    this->free(remote_ptr(tup->first, 0));
    void *mem = std::get<1>(tup->second);
    munmap(mem, std::get<2>(tup->second));
    shm_unlink(std::get<3>(tup->second).c_str());
    device2virtual.erase(device2virtual.begin());
  }
}


rocc_response fpga_handle_t::get_response_from_handle(int handle) const {
  int rc = pthread_mutex_lock(&cmd_server->wait_for_response[handle]);
  // command is now ready
  auto resp = cmd_server->responses[handle];
  // now that we've read our response, we can release the resource to be used in future responses
  rc |= pthread_mutex_lock(&cmd_server->free_list_lock);
  cmd_server->free_list[++cmd_server->free_list_idx] = handle;
  rc |= pthread_mutex_unlock(&cmd_server->free_list_lock);
  if (rc) {
    printf("Error in fpga_handle.cc:get_response_from_handle\t%s\n", strerror(errno));
    exit(rc);
  }
  return resp;
}

std::optional<rocc_response> fpga_handle_t::try_get_response_from_handle(int handle) const {
  int rc = pthread_mutex_trylock(&cmd_server->wait_for_response[handle]);
  if (rc) return {};
  // command is now ready
  auto resp = cmd_server->responses[handle];
  // now that we've read our response, we can release the resource to be used in future responses
  rc |= pthread_mutex_lock(&cmd_server->free_list_lock);
  cmd_server->free_list[++cmd_server->free_list_idx] = handle;
  rc |= pthread_mutex_unlock(&cmd_server->free_list_lock);
  if (rc) {
    printf("Error in fpga_handle.cc:get_response_from_handle\t%s\n", strerror(errno));
    exit(rc);
  }
  return resp;
}


response_handle<rocc_response> fpga_handle_t::send(const rocc_cmd &c) {
  // acquire lock over client side
  int error = pthread_mutex_lock(&cmd_server->cmd_send_lock);
  // communicate data to shared space
  cmd_server->cmd = c;
//  std::cout << "command in file is " << cmd_server->cmd << std::endl;
  // signal to server that we have a command ready
  error |= pthread_mutex_unlock(&cmd_server->server_mut);
  // wait for server to signal that it has read our command
  error |= pthread_mutex_lock(&cmd_server->cmd_recieve_server_resp_lock);
  // get the handle that we use to wait for response asynchronously
  int handle = cmd_server->pthread_wait_id;
  // release lock over client side
  error |= pthread_mutex_unlock(&cmd_server->cmd_send_lock);

  cmd_server->quit = false;
  if (error) {
    printf("Error in send: %s\n", strerror(errno));
    fflush(stdout);
    exit(1);
  }
  return response_handle<rocc_response>(c.getXd(), handle, *this);
}

// =====================================================================
// Two memory paths: real Zynq silicon vs simulation
// =====================================================================
// Real silicon: the FPGA fabric and the CPU share the same DDR controller
// on-chip. We allocate a host buffer, lock it so the kernel can't page
// it out, look up its physical address (vtop, see /proc/self/pagemap
// reader above), and hand the FPGA that physical address. The runtime
// daemon never touches the data path; it only ferries commands.
//
// Simulation: there is no FPGA. The "FPGA" is a software model
// (Verilator / Icarus / VCS) and its "DDR" is DRAMsim3's internal
// byte array — separate from host RAM. So a host pointer + a
// /proc/self/pagemap-derived physical address means nothing to the
// simulator. Instead, every allocation is registered with the runtime
// daemon's data_server, which maintains an address translator (AT)
// mapping fpga_addr <-> a shmem region that both client and daemon
// (and the simulator, via the AT) see. This is exactly what
// fpga_handle_discrete.cc already does.
//
// libbeethoven-zynq.so is built once globally (no SIM/non-SIM split),
// so we cannot decide which path to take at compile time. The runtime
// daemon publishes its mode via data_server->is_simulation (set by
// runtime/src/core/data_server.cc on startup). We branch on that flag
// at the start of each public method.
// =====================================================================

remote_ptr fpga_handle_t::malloc(size_t len) {
  if (data_server->is_simulation) {
    // Sim: ask the daemon to carve out a shmem-backed region and add it
    // to the address translator. Mirrors fpga_handle_discrete.cc::malloc.
    pthread_mutex_lock(&data_server->data_cmd_send_lock);
    data_server->op_argument = len;
    data_server->operation = ALLOC;
    pthread_mutex_unlock(&data_server->server_mut);
    // wait for the daemon to populate fname + the chosen fpga_addr
    pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
    uint64_t fpga_addr = data_server->op_argument;

    int fd = shm_open(data_server->fname, O_RDWR, file_access_flags);
    if (fd < 0) {
      throw std::runtime_error("Failed to open shmem file " +
                               std::string(data_server->fname) + " - " +
                               std::string(strerror(errno)));
    }
    void *ptr = mmap(nullptr, len, file_access_prots, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      throw std::runtime_error("Failed to map shmem file " +
                               std::string(data_server->fname) + " - " +
                               std::string(strerror(errno)));
    }
    // Track for the destructor's cleanup pass (munmap + shm_unlink).
    device2virtual[fpga_addr] =
        std::make_tuple(fd, ptr, len, std::string(data_server->fname));
    pthread_mutex_unlock(&data_server->data_cmd_send_lock);
    return remote_ptr(fpga_addr, ptr, len);
  }

  // Real silicon: prefer the kernel reserved DMA heap. On ZynqMP this gives
  // low, contiguous, PL-accessible DDR; anonymous hugetlb mappings can land
  // above 4 GiB and have proven unreliable for PL write channels on AUP-ZU3.
  try {
    return try_dma_heap_malloc(len);
  } catch (const std::runtime_error &ex) {
    std::cerr << "Warning: falling back to hugetlb FPGA malloc: " << ex.what() << std::endl;
  }

  // Fallback: host mmap + lock + vtop. The FPGA reads straight from
  // the locked page; the daemon plays no role in the data path.
  void *addr;
  size_t sz;
  if (len <= 1 << 12) {
    sz = 1 << 12;
    addr = mmap(nullptr, sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0);
  } else {
    // pick the smallest huge-page size class that fits the request
    int fit = -1;
    for (int i = 0; i < zynq_n_page_sizes && fit == -1; ++i) {
      if (len <= zynq_huge_page_sizes[i])
        fit = i;
    }
    if (fit == -1)
      throw std::runtime_error("Error in FPGA malloc: no huge page size available for allocation of size "
                                + std::to_string(len) + "B");
    addr = mmap(nullptr, zynq_huge_page_sizes[fit], PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_HUGETLB | zynq_huge_page_flags[fit] | MAP_LOCKED | MAP_ANONYMOUS,
                -1, 0);
    sz = zynq_huge_page_sizes[fit];
  }

  if (addr == MAP_FAILED)
    throw std::runtime_error("Error in FPGA malloc. Map failed for alloc of size (" + std::to_string(double(len)/1024/1024) + ") MB. Err msg: " + std::string(strerror(errno)));

  if (mlock(addr, sz))
    throw std::runtime_error("Error in FPGA malloc. mlock failed. Err msg: " + std::string(strerror(errno)));
  return remote_ptr(vtop((intptr_t) addr), addr, sz);
}

[[maybe_unused]] void fpga_handle_t::copy_to_fpga(const remote_ptr &dst) {
  if (data_server->is_simulation) {
    // Sim: tell the daemon to register the move (the buffer is already
    // shmem-mapped, so the simulator reads through the AT — this op
    // just synchronizes the bookkeeping).
    pthread_mutex_lock(&data_server->data_cmd_send_lock);
    data_server->operation = data_server_op::MOVE_TO_FPGA;
    data_server->op_argument = dst.getFpgaAddr();
    data_server->op3_argument = dst.getLen();
    pthread_mutex_unlock(&data_server->server_mut);
    pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
    pthread_mutex_unlock(&data_server->data_cmd_send_lock);
    return;
  }
  // Real silicon: flush CPU cache so the FPGA sees fresh data in DDR.
  arm_dcache_flush(dst.getHostAddr(), dst.getLen());
}

[[maybe_unused]] void fpga_handle_t::copy_from_fpga(const remote_ptr &src) {
  if (data_server->is_simulation) {
    // Sim: same idea — the buffer is shmem-mapped, just synchronize.
    pthread_mutex_lock(&data_server->data_cmd_send_lock);
    data_server->operation = data_server_op::MOVE_FROM_FPGA;
    data_server->op2_argument = src.getFpgaAddr();
    data_server->op3_argument = src.getLen();
    pthread_mutex_unlock(&data_server->server_mut);
    pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
    pthread_mutex_unlock(&data_server->data_cmd_send_lock);
    return;
  }
  // Real silicon: invalidate CPU cache so we re-read from DDR.
  arm_dcache_invalidate(src.getHostAddr(), src.getLen());
}

[[maybe_unused]] void fpga_handle_t::free(remote_ptr ptr) {
  if (data_server->is_simulation) {
    // Sim: ask the daemon to drop the AT entry. The destructor still
    // does the munmap + shm_unlink for entries in device2virtual.
    pthread_mutex_lock(&data_server->data_cmd_send_lock);
    data_server->op_argument = ptr.getFpgaAddr();
    data_server->operation = FREE;
    pthread_mutex_unlock(&data_server->server_mut);
    pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
    pthread_mutex_unlock(&data_server->data_cmd_send_lock);
    return;
  }
  // Real silicon: just unmap the host page.
  munmap(ptr.getHostAddr(), ptr.getLen());
}

[[maybe_unused]] void fpga_handle_t::shutdown() const {
  pthread_mutex_lock(&cmd_server->cmd_send_lock);
  pthread_mutex_unlock(&cmd_server->server_mut);
  cmd_server->quit = true;
  pthread_mutex_unlock(&cmd_server->cmd_send_lock);
}

[[maybe_unused]] void fpga_handle_t::request_startup() {
  // first, check that there isn't already a BeethovenRuntime process running
  // if there is, then we don't need to do anything

  // use ps utility to see if process is running
  // if it is, then we don't need to do anything

  // this is rough but maybe it can work alright
  system("killall BeethovenRuntime");

  // try to build and startup the beethoven runtime FOR SIMULATION ONLY
  // record current working directory
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  // change to beethoven runtime directory
  // get $BEETHOVEN_ROOT environment variable
  char *beethoven_root = getenv("BEETHOVEN_ROOT");
  // change to beethoven_root/Beethoven-Hardware/vsim/generated-src
  chdir(beethoven_root);
  chdir("Beethoven-Runtime");
  // make a build directory if one does not exist
  mkdir("build", 0777);
  // change to build directory
  chdir("build");
  // run cmake
  system("cmake .. -DTARGET=sim -DCMAKE_BUILD_TYPE=Debug");
  // run make
  system("make -j");
  // fork exec the executable that was just built (./BeethovenRuntime)
  // output system out to log file in the same directory (Beethoven.log)
  // then change back to original directory
  // fork and exec
  printf("FORKING\n");
  pid_t pid = fork();
  if (pid == 0) {
    // child
    // redirect stdout and stderr to log file
    int fd = open("Beethoven.log", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
    // exec
    execl("./BeethovenRuntime", "./BeethovenRuntime", nullptr);
    // if we get here, exec failed
    std::cerr << "Failed to exec BeethovenRuntime" << std::endl;
    exit(1);
  } else if (pid < 0) {
    // fork failed
    std::cerr << "Failed to fork BeethovenRuntime" << std::endl;
    exit(1);
  } else {
    // parent
    // change back to original directory
    chdir(cwd);
  }
  sleep(1);
  printf("Returning\n");
}
