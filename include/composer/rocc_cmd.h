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

#include <zconf.h>
#include "composer_consts.h"
#include <cstdint>
#include "alloc.h"
#include <ostream>
#include "response_handle.h"
#include "fpga_handle.h"

namespace composer {
  enum channel {
    write = 0,
    read = 1
  };
  class rocc_cmd {
    /**
     * Generate a command to start kernel execution on the accelerator. TODO: maybe an accelerator
     * should be able to support more than one command per functional unit. You could of course do that
     * by commandeering another instruction field (e.g. rs1) but it would be more natural to use the
     * function bits
     * @param system_id
     * @param rs1_num
     * @param rs2_num
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
    uint8_t rs1_num;
    uint8_t rs2_num;
    uint8_t xd;
    RD rd;
    uint8_t xs1;
    uint8_t xs2;
    uint8_t core_id;
    uint64_t rs1;
    uint64_t rs2;

    bool is_addr_cmd;

    rocc_cmd(uint16_t function, uint16_t systemId, uint8_t opcode, uint8_t rs1Num, uint8_t rs2Num, uint8_t xd, RD rd,
             uint8_t xs1, uint8_t xs2, uint8_t coreId, uint64_t rs1, uint64_t rs2);

    rocc_cmd(uint16_t function, uint16_t systemId, uint8_t opcode, uint8_t coreId, uint64_t addr, uint64_t length,
             uint8_t channel_id, channel channel_ty);

  public:

    static rocc_cmd flush_cmd();

    static rocc_cmd start_cmd(
            uint16_t system_id,
            uint8_t rs1_num,
            uint8_t rs2_num,
            bool expect_response,
            RD rd,
            uint8_t xs1,
            uint8_t xs2,
            uint8_t core_id,
            uint64_t rs1,
            uint64_t rs2);

    static rocc_cmd addr_cmd(
            uint16_t system_id,
            uint8_t core_id,
            uint8_t channel_id,
            composer::channel channel_ty,
            const composer::remote_ptr &ptr);

    [[nodiscard]] uint32_t *pack(const composer_pack_info &info) const;

    [[nodiscard]] uint16_t getFunction() const;

    [[nodiscard]] uint16_t getSystemId() const;

    [[nodiscard]] uint8_t getOpcode() const;

    [[nodiscard]] uint8_t getRs1Num() const;

    [[nodiscard]] uint8_t getRs2Num() const;

    [[nodiscard]] uint8_t getXd() const;

    [[nodiscard]] RD getRd() const;

    [[nodiscard]] uint8_t getXs1() const;

    [[nodiscard]] uint8_t getXs2() const;

    [[nodiscard]] uint8_t getCoreId() const;

    [[nodiscard]] uint64_t getRs1() const;

    [[nodiscard]] uint64_t getRs2() const;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"
    response_handle send() const;
#pragma clang diagnostic pop
  };
}

std::ostream &operator<<(std::ostream &os, const composer::rocc_cmd &cmd);

#endif //ROCC_H
