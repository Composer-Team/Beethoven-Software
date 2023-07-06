/*
 * Copyright (c) 2022,
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

#ifndef COMPOSER_ALLOC_H
#define COMPOSER_ALLOC_H

#include <cinttypes>
#include <array>
#include <tuple>
#include <set>
#include <vector>
#include <pthread.h>
#include <iostream>
#include <algorithm>
#include "fpga_handle.h"

namespace composer {
  class remote_ptr {
    // NOTE: For security purposes we could probably pack this structure full of information that guarantees that the
    //       user is behaving
    uint64_t fpga_addr;
    void * host_addr;
    uint32_t len;
  public:
    bool freed;

    explicit remote_ptr(size_t size) {
      auto me = current_handle_context->malloc(size);
      fpga_addr = me.getFpgaAddr();
      host_addr = me.getHostAddr();
      len = me.getLen();
      freed = false;
    }

    [[nodiscard]] uint64_t getFpgaAddr() const {
      return fpga_addr;
    }

    [[nodiscard]] uint32_t getLen() const {
      return len;
    }

    [[nodiscard]] void *getHostAddr() const {
      return host_addr;
    }

    explicit remote_ptr(uint64_t fpgaAddr, uint32_t len) : fpga_addr(fpgaAddr), len(len), freed(false), host_addr(nullptr) {}

    explicit remote_ptr(uint64_t fpgaAddr, void *hostAddr, uint32_t len) : fpga_addr(fpgaAddr), host_addr(hostAddr), len(len), freed(false){}

    explicit remote_ptr() : fpga_addr(0), host_addr(nullptr), len(0), freed(false) {}

    [[nodiscard]] std::string printError();

  };

  const int ERR_ALLOC_TOO_BIG = 0x1;
  const int ERR_MMAP_FAILURE = 0x2;
  const int ERR_MLOCK_FAILURE = 0x4;

  static constexpr uint8_t log2up(const uint64_t &val) {
    uint64_t acc = 1;
    for (unsigned i = 0; i < 64; ++i) {
      if (val <= acc)
        return i;
      acc <<= 1;
    }
    return 64;
  }

  template <uint64_t size,
          uint32_t superblock_size = (1 << 21),
          uint32_t min_block_size = (1 << 12)>
  class device_allocator {
    uint64_t _sz = size;
    uint32_t _sbsz = superblock_size;
    uint32_t _mbsz = min_block_size;
    static constexpr uint8_t addr_bits = log2up(size);
    static constexpr uint8_t log_min_block = log2up(min_block_size);
    static constexpr uint8_t log_superblock_size = log2up(superblock_size);
    static constexpr uint8_t num_lists = log_superblock_size - log_min_block;
    static constexpr uint64_t log_num_superblocks = addr_bits - log_superblock_size;

    static constexpr uint64_t superblock_id_mask() {
      uint64_t mask = 1;
      for (unsigned i = 0; i < log_num_superblocks; ++i) {
        mask = (mask << 1) | 1;
      }
      return mask;
    }

    static constexpr uint64_t block_id_mask(uint8_t log_block_size) {
      uint64_t mask = 1;
      for (unsigned i = 0; i < log_block_size; ++i) {
        mask = (mask << 1) | 1;
      }
      return mask;

    }

    /**
     * not a concatenation of superblock aka small alloc
     */
    static const int BLOCK_ALLOC = 0x1;
    /**
     * Only valid if not block-alloc. Signifies whether this superblock is the first superblock in the allocation
     */
    static const int BASE_ALLOCATION = 0x2;

    // idx of beginning of unallocated space
    uint32_t superblock_free_idx = 0;
    static constexpr int superblock_free_idx_lim = 1 << (addr_bits - log_superblock_size);
    pthread_mutex_t superblock_free_idx_lock = PTHREAD_MUTEX_INITIALIZER;

    // free list that's not contiguous with upper address space
    std::vector<uint32_t> free_list;
    pthread_mutex_t free_list_lock = PTHREAD_MUTEX_INITIALIZER;

    struct superblock {
      uint8_t log_block_size = 0;
      uint8_t flags = 0;
      std::vector<uint16_t> free_blocks;
      pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

      superblock(uint16_t logBlockSize, uint64_t base) : log_block_size(logBlockSize) {}

      // prioritize superblocks with less room
      [[nodiscard]] uint64_t priority() const {
        if (free_blocks.empty())
          return superblock_size;
        else return free_blocks.size();
      }

      superblock() = default;
    };

    std::array<superblock, 1 << log_num_superblocks> superblocks;

    struct superblock_ptr {
      uint32_t sb_id;

      bool operator<(const superblock_ptr &other) const {
        return sb_id < other.sb_id;
      }

      explicit superblock_ptr(uint32_t superblock_id) : sb_id(superblock_id) {}
    };
    // we always want to keep the largest allocations available as possible. To do this, we prioritize making allocations
    // near address 0. This way, frees are more likely to completely free high addressed superblocks and reduce
    // free_slab_idx. NOTE: This may not be the right way to do this TODO UG

    // TODO HOW DO WE DEAL WITH SECURITY? consider notes in composer_alloc.h - just load the remote_ptr with data that
    //  can be double checked with composer-runtime's state
    std::array<std::set<superblock_ptr>, num_lists> superblock_head_list;
    std::array<pthread_mutex_t, num_lists> head_locks;

    void unpack_free_list_THREAD_UNSAFE() {
      // FREE LIST MUST BE LOCKED BEFORE CALLING THIS
      while (not free_list.empty()) {
        if (free_list.back() + 1 == superblock_free_idx) {
          --superblock_free_idx;
          free_list.pop_back();
        } else break;
      }
    }


    struct block_info {
      uint32_t superblock_id, block_id;
      block_info(uint32_t sid, uint32_t bid): superblock_id(sid), block_id(bid) {}
    };
    block_info get_block_info(const remote_ptr &ptr, bool verbose=false) {
      if (verbose) {
        printf("addr: %16x\n", ptr.getFpgaAddr());
      }
      uint32_t superblock_id = (ptr.getFpgaAddr() >> log_superblock_size) & superblock_id_mask();
      auto &sb = superblocks[superblock_id];
      if (sb.flags & BLOCK_ALLOC) {
        auto log_block_size = sb.log_block_size;
        uint32_t block_id = (ptr.getFpgaAddr() >> log_block_size) & block_id_mask(log_superblock_size);
        return block_info(superblock_id, block_id);
      } else return block_info(superblock_id, 0);
    }

  public:
    remote_ptr malloc(uint64_t len) {
      const auto log_block_size = std::max(log2up(len), log_min_block);
      uint64_t block_size = 1 << log_block_size;
      uint32_t idx;
      if (log_block_size < log_min_block) {
        idx = 0;
      } else {
        idx = log_block_size - log_min_block;
      }
      if (idx < num_lists) {
        // can allocate a block inside a superblock
        uint64_t superblock_id;
        uint64_t block_id;
        pthread_mutex_t &my_head_lock = head_locks[idx];
        pthread_mutex_lock(&my_head_lock);
        auto &sblist = superblock_head_list[idx];
        if (sblist.empty()) {
          // no superblocks currently with this block size
          // see if there are any superblocks in free list. otherwise allocate from upper address space
          pthread_mutex_lock(&free_list_lock);
          pthread_mutex_lock(&superblock_free_idx_lock);
          if (not free_list.empty()) {
            // free list has blocks, acquire and remove from list
            superblock_id = free_list.front();
            free_list.erase(free_list.begin());
          } else {
            // free list is empty, acquire new block from upper address space
            superblock_id = superblock_free_idx++;
          }
          pthread_mutex_unlock(&superblock_free_idx_lock);
          pthread_mutex_unlock(&free_list_lock);
          auto &my_sb = superblocks[superblock_id];
          // initialize superblock
          my_sb.log_block_size = log_block_size;
          my_sb.free_blocks.clear();
          uint64_t n_blocks = 1 << (log_superblock_size - log_block_size);
          if (n_blocks >= 0x10000) {
            fprintf(stderr, "FATAL ERROR - more blocks than allowed in superblock\n");
            exit(1);
          }
          my_sb.flags = BLOCK_ALLOC;
          for (int i = 0; i < (1 << (log_superblock_size - log_block_size)); ++i)
            my_sb.free_blocks.push_back(i);
          my_sb.lock = PTHREAD_MUTEX_INITIALIZER;

          // assign it to the head of the list
          superblock_head_list[idx].emplace(superblock_id);
          block_id = my_sb.free_blocks.back();
          my_sb.free_blocks.pop_back();
          pthread_mutex_unlock(&my_head_lock);
        } else {
          // else, there's space in a current superblock
          superblock_id = sblist.begin()->sb_id;
          auto &my_sb = superblocks[superblock_id];
          // lock so we don't compete with frees
          pthread_mutex_lock(&my_sb.lock);

          // acquire block and remove from list
          block_id = my_sb.free_blocks.back();
          my_sb.free_blocks.pop_back();

          // it might be empty, then take it off the head list. Subsequent frees will add it back to head list
          if (my_sb.free_blocks.empty()) {
            sblist.erase(sblist.begin());
          }
          pthread_mutex_unlock(&my_sb.lock);
          pthread_mutex_unlock(&my_head_lock);
        }
//        printf("sbid: %lu, bid: %lu, flags: %d\n", superblock_id, block_id, superblocks[superblock_id].flags); fflush(stdout);
        return remote_ptr(superblock_id * superblock_size + block_id * block_size, len);
      } else {
        // we're just going to use a huge allocation (not within a superblock)
        uint32_t num_superblocks_needed = 1 << (log_block_size - log_superblock_size);
        // try looking for contiguous superblocks in free list before we resort to allocating fresh memory
        // try for best fit
        // we need to store size of allocation inside free-list to use when we're freeing. Since the block_id is 16b,
        //  we also need the length of the superblock allocation to be 16b, limiting us to allocations that are
        //  2MB * 2^16 long = 2^21 * 2^16 = 2^37 = 128GB. If this is a problem, it can easily be changed. Or maybe the
        //  user should just make multiple allocations. It might run into trouble with readers though that attempt to
        //  walk over an allocation boundary (that's how I'm planning to implement security right now).
        if (num_superblocks_needed & 0xFFFF0000) {
          // too many!
          fprintf(stderr, "FATAL - Composer does not currently support allocations larger than 128GB."
                          " Notify developers and we can do something about it with some ease.\n");
          exit(1);
        }
        int current_len = 0;
        uint32_t current_seg_begin = 0;
        pthread_mutex_lock(&free_list_lock);
        std::vector<std::pair<uint32_t, int> > segments;
        auto it = free_list.begin();
        while (it != free_list.end()) {
          current_len += 1;
          auto it_next = it + 1;
          if (it_next == free_list.end()) {
            segments.emplace_back(current_seg_begin, current_len);
            // gonna quit after this
          } else if (*it_next != (*it + 1)) {
            // not continuous, segment breaks
            segments.emplace_back(current_seg_begin, current_len);
            current_len = 0;
            current_seg_begin = *it_next;
          }
          it = it_next;
          // else continuous, we can keep going
        }

        // now we have all the segments, find the smallest one that fits the allocation
        int best_size = 1 << log_num_superblocks;
        uint32_t alloc_start;
        bool success = false;
        while (!segments.empty()) {
          uint32_t segment_start;
          int segment_size;
          std::tie(segment_start, segment_size) = segments.back();
          // prioritize segments that start earlier in address space if there are equal length options
          if (segment_size >= num_superblocks_needed and
              (segment_size < best_size or (segment_size == best_size and segment_start < alloc_start))) {
            best_size = segment_size;
            alloc_start = segment_start;
            success = true;
          }
        }
        if (success) {
          // then we can use this segment instead of allocating new memory
          // remove it from free list
          auto alloc_start_it = std::find(free_list.begin(), free_list.end(), alloc_start);
          // we can use erase range since all elements are contiguous
          free_list.erase(alloc_start_it, alloc_start_it + num_superblocks_needed);
          pthread_mutex_unlock(&free_list_lock);
          // initialize superblocks outside this control statement
        } else {
          // failed to find something suitable in free_list, try to allocate new memory instead
          pthread_mutex_unlock(&free_list_lock);
          pthread_mutex_lock(&superblock_free_idx_lock);
          // ensure there's enough space before we acquire it
          if (superblock_free_idx + num_superblocks_needed > superblock_free_idx_lim) {
            pthread_mutex_unlock(&superblock_free_idx_lock);
            fprintf(stderr, "FATAL ERROR - out of memory in composer allocator\n");
            exit(1);
          }

          alloc_start = superblock_free_idx;
          superblock_free_idx += num_superblocks_needed;
          pthread_mutex_unlock(&superblock_free_idx_lock);
        }
        superblocks[alloc_start].flags = BASE_ALLOCATION;
        // need to store size of allocation so it can be freed
        superblocks[alloc_start].free_blocks.clear();
        superblocks[alloc_start].free_blocks.push_back(num_superblocks_needed);
        for (uint32_t i = 1; i < num_superblocks_needed; i++) {
          superblocks[alloc_start + i].flags = 0; // they will not be marked as base allocations
        }
        return remote_ptr(alloc_start * superblock_size, len);
      }
    }

    void free(remote_ptr p) {
      // TODO security - make sure this pointer belongs to the user who's freeing it
      auto bi = get_block_info(p);
      auto &sb = superblocks[bi.superblock_id];
      if (p.freed) {
        fprintf(stderr, "ERROR - attempting to double-free\n");
        exit(1);
      }
      p.freed = true;
      // sanity check flags
      auto flags = sb.flags;
      if ((flags & BLOCK_ALLOC) && (flags & BASE_ALLOCATION)) {
        fprintf(stderr, "WARNING - incoherent flags in superblocks in composer allocator\n");
      }
      printf("free info: %d %d %d\n", bi.superblock_id, bi.block_id, flags);
      if (flags & BLOCK_ALLOC) {
        // small allocation
        pthread_mutex_lock(&sb.lock);
        sb.free_blocks.push_back(bi.block_id);
        auto max_occ = 1 << (log_superblock_size - sb.log_block_size);
        // if superblock is entirely unoccupied, add it to free list
        bool is_unoccupied = sb.free_blocks.size() == max_occ;
        // if the block was entirely allocated and there's something free in it again, we need to make it allocatable
        bool is_newly_populated = sb.free_blocks.size() == 1;
        uint8_t lbs = sb.log_block_size;
        pthread_mutex_unlock(&sb.lock);

        // LOTS of race conditions and possibilities for deadlock (are those the same) here, gotta be careful by
        // un-acquiring all locks and re-acquiring one-at-a-time
        if (is_unoccupied || is_newly_populated) {
          uint8_t head_idx = lbs - log_min_block;
          if (head_idx >= num_lists) {
            fprintf(stderr, "FATAL - Invalid block information found in composer allocator block %d %d\n",
                    log_min_block, lbs);
            exit(1);
          }
          pthread_mutex_lock(&head_locks[head_idx]);
          pthread_mutex_lock(&sb.lock);
          if (sb.free_blocks.size() == max_occ) {
            // no one has since acquired a block inside this superblock
            // acquiring head lock mean that no one can currently have a pointer to this superblock
            auto &pq = superblock_head_list[head_idx];
            pq.erase(pq.begin());
            pthread_mutex_lock(&superblock_free_idx_lock);
            if (bi.superblock_id + 1 == superblock_free_idx) {
              // decrement free_idx and don't insert into free list (it's implied)
              superblock_free_idx--;
              // go backwards in list and free up as many superblocks as possible to the upper address space region
              unpack_free_list_THREAD_UNSAFE();
            } else {
              // otherwise we're not the highest address allocated and wont be able to decrement the superblock_free_idx
              free_list.insert(std::upper_bound(free_list.begin(), free_list.end(), bi.superblock_id),
                               bi.superblock_id);
            }
            pthread_mutex_unlock(&superblock_free_idx_lock);
          } else if (sb.free_blocks.size() == 1) {
            // handling newly populated
            // wasn't in head list so we need to insert it into list
            superblock_head_list[head_idx].emplace(bi.superblock_id);
          }
          pthread_mutex_unlock(&sb.lock);
          pthread_mutex_unlock(&head_locks[head_idx]);
        }
      } else {
        // not a block allocation. This is part of a super-block
        int64_t id = bi.superblock_id;
        // if, for some reason, we weren't given the pointer to the head superblock, find it
        // NOTE for security - make sure all of these blocks belong to user
        if (not(sb.flags & BASE_ALLOCATION)) {
          // not given the base superblock in free, just walk down till we find it
          id = bi.superblock_id--;
          while (id >= 0 && not(superblocks[id].flags & BASE_ALLOCATION)) id--;
        }
        if (id < 0) {
          fprintf(stderr, "FATAL - base allocation not found...\n");
          exit(1);
        }
        if (sb.flags & BLOCK_ALLOC) {
          fprintf(stderr, "FATAL - given allocation with inconsistent state\n");
          exit(1);
        }
        auto n_superblocks = superblocks[id].free_blocks.front();
        superblocks[id].free_blocks.clear();
        pthread_mutex_lock(&superblock_free_idx_lock);
        if (id + n_superblocks + 1 == superblock_free_idx) {
          // allocation is right up against upper address barrier. Put as many free blocks as possible into free area
          superblock_free_idx = id;
          unpack_free_list_THREAD_UNSAFE();
        } else {
          // otherwise, add each individual superblock into free-list
          for (uint16_t i = 0; i < n_superblocks; ++i) {
            free_list.emplace_back(id + i);
          }
        }
        pthread_mutex_unlock(&superblock_free_idx_lock);
      }
    }

    device_allocator() {
      for (auto &l: head_locks) l = PTHREAD_MUTEX_INITIALIZER;
    };
  };
}

#endif //COMPOSER_ALLOC_H
