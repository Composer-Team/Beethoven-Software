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

#include <composer/rocc_response.h>
#include <composer/verilator_server.h>
#include <map>
#include <vector>
#include <composer/allocator_ptr.h>

namespace composer {
  class response_handle;

  class rocc_cmd;

  class rocc_reponse;

  struct fpga_handle_t {
  private:
    friend response_handle;

    [[nodiscard]] rocc_response get_response_from_handle(int handle) const;

  private:
    cmd_server_file *cmd_server;
    int csfd;
    data_server_file *data_server;
    int dsfd;
    std::map<uint64_t, std::tuple<int, void *, int, std::string> > device2virtual;

    std::vector<const remote_ptr *> to_flush;

  public:
    /**
     * flush all in-flight commands
     */
    rocc_response flush();

    explicit fpga_handle_t();

    /**
     * @brief send a command to the FPGA
     * @return handle referring to response that the command will return. Allows for blocking on the response.
     */

    [[nodiscard]] response_handle send(const rocc_cmd &c);

    ~fpga_handle_t();

    composer::remote_ptr malloc(size_t len);

    void flush_data_to_fpga();

    void copy_to_fpga(const composer::remote_ptr &dst);

    void copy_from_fpga(const composer::remote_ptr &src);

    void free(composer::remote_ptr fpga_addr);

    void shutdown() const;
  };

  extern std::vector<fpga_handle_t *> active_fpga_handles;

  extern fpga_handle_t *current_handle_context;

  void set_fpga_context(fpga_handle_t *t);
}
#endif
