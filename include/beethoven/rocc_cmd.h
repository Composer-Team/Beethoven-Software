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


#ifndef ROCC_H
#define ROCC_H

#include <cinttypes>
#include <beethoven/beethoven_consts.h>
#include <beethoven/util.h>
#include <beethoven/response_handle.h>
//#include "include/beethoven/allocator/alloc.h"

namespace beethoven {
  class fpga_handle_t;

  class rocc_cmd {
    friend fpga_handle_t;
    /**
     * Generate a command to start kernel execution on the accelerator.
     * @param system_id
     * @param xd
     * @param rd
     * @param xs1
     * @param xs2
     * @param core_id
     * @param rs1
     * @param rs2
     * @return
     */
    uint16_t function;
    uint16_t system_id;
    uint8_t opcode;
    uint8_t xd;
    uint8_t rd;
    uint8_t xs1;
    uint8_t xs2;
    uint16_t core_id;
    uint64_t rs1;
    uint64_t rs2;


    rocc_cmd(uint16_t function, uint16_t systemId, uint8_t opcode, uint8_t xd, uint8_t rd,
             uint8_t xs1, uint8_t xs2, uint16_t coreId, uint64_t rs1, uint64_t rs2);


  public:
    rocc_cmd(const rocc_cmd &other) = default;

    static rocc_cmd start_cmd(
            uint16_t system_id,
            bool expect_response,
            uint8_t rd,
            uint8_t xs1,
            uint8_t xs2,
            uint16_t core_id,
            uint64_t rs1,
            uint64_t rs2,
            uint16_t function_id);

    void pack(const beethoven_pack_info &info, uint32_t *ar, uint8_t rd_override=255) const;

    [[nodiscard]] uint16_t getFunction() const;

    [[nodiscard]] uint16_t getSystemId() const;

    [[nodiscard]] uint8_t getOpcode() const;

    [[nodiscard]] uint8_t getXd() const;

    [[nodiscard]] uint8_t getRd() const;

    [[nodiscard]] uint8_t getXs1() const;

    [[nodiscard]] uint8_t getXs2() const;

    [[nodiscard]] uint8_t getCoreId() const;

    [[nodiscard]] uint64_t getRs1() const;

    [[nodiscard]] uint64_t getRs2() const;

    [[maybe_unused]] response_handle<rocc_response> send() const; // NOLINT(*-use-nodiscard)
  };
}

#ifndef BAREMETAL
std::ostream &operator<<(std::ostream &os, const beethoven::rocc_cmd &cmd);
#endif

#endif //ROCC_H
