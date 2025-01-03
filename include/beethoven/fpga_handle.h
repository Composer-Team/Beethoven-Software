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

#ifndef FPGA_HANDLE_H
#define FPGA_HANDLE_H

#include <beethoven/rocc_response.h>
#include <beethoven/response_handle.h>
#include <cstddef>

#ifndef BAREMETAL
#include <beethoven/verilator_server.h>
#include <map>
#include <vector>
#include "beethoven/allocator/alloc.h"
#else
#include "beethoven/allocator/alloc_baremetal.h"
#endif

namespace beethoven {
  class response_getter;

  class rocc_cmd;

  struct remote_ptr;

  struct fpga_handle_t {
  private:
    friend response_getter;
#ifndef BAREMETAL

    [[nodiscard]] rocc_response get_response_from_handle(int handle) const;

    [[nodiscard]] std::optional<rocc_response> try_get_response_from_handle(int handle) const;

  private:
    cmd_server_file *cmd_server;
    int csfd;
    data_server_file *data_server;
    int dsfd;
    std::map<uint64_t, std::tuple<int, void *, int, std::string> > device2virtual;
#endif

  public:

    explicit fpga_handle_t();

    /**
     * @brief send a command to the FPGA
     * @return handle referring to response that the command will return. Allows for blocking on the response.
     */

    [[nodiscard]] response_handle<rocc_response> send(const rocc_cmd &c);

    ~fpga_handle_t();

    remote_ptr malloc(size_t len);

    [[maybe_unused]] void free(remote_ptr fpga_addr);

    [[maybe_unused]] void copy_to_fpga(const remote_ptr &dst);

    [[maybe_unused]] void copy_from_fpga(const remote_ptr &src);


#ifndef BAREMETAL


    [[maybe_unused]] static void request_startup();

    [[maybe_unused]] void shutdown() const;
#endif
  };

#ifndef BAREMETAL
  extern std::vector<fpga_handle_t *> active_fpga_handles;

  extern fpga_handle_t *current_handle_context;

  [[maybe_unused]] void set_fpga_context(fpga_handle_t *t);
#endif
}
#endif
