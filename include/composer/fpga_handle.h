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

#include <vector>
#include "util.h"

namespace composer {
  class response_handle;
  class rocc_cmd;

  struct rocc_response {
    uint64_t data;
    uint8_t core_id;
    uint8_t system_id;
    uint8_t rd;
  private:
    static uint64_t get_mask(int l) {
      uint64_t mask = 0;
      for (int i = 0; i < l; ++i)
        mask = (mask << 1) | 1;
      return mask;
    }
  public:
    rocc_response(const uint32_t *buffer, const composer_pack_info &pack_info);

  };

  struct fpga_handle_t {
  private:
    friend response_handle;
    [[nodiscard]] virtual rocc_response get_response_from_handle(int handle) const = 0;

  public:

    // return if the handle refers to a real FPGA or not
    [[nodiscard]] virtual bool is_real() const = 0;

    /**
     * @brief send a command to the FPGA
     * @return handle referring to response that the command will return. Allows for blocking on the response.
     */
    [[nodiscard]] virtual response_handle send(const rocc_cmd &c) const = 0;

    /**
     * flush all in-flight commands
     */
    rocc_response flush();
  };

  extern std::vector<fpga_handle_t*> active_fpga_handles;

  extern fpga_handle_t* current_handle_context;

  void set_fpga_context(fpga_handle_t *t);
}
#endif
