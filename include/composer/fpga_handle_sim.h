//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "rocc_cmd.h"
#include "fpga_handle.h"
#include "verilator_server.h"
#include <map>

#ifndef COMPOSER_FPGA_HANDLE_SIM_H
#define COMPOSER_FPGA_HANDLE_SIM_H

namespace composer {
  class fpga_handle_sim_t : public fpga_handle_t {
  private:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"
    rocc_response get_response_from_handle(int handle) const override;
#pragma clang diagnostic pop

  public:
    explicit fpga_handle_sim_t();

#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"
    response_handle send(const rocc_cmd &c) const override;
#pragma clang diagnostic pop

    [[nodiscard]] bool is_real() const override;

    ~fpga_handle_sim_t();

    composer::remote_ptr malloc(size_t len);

    void copy_to_fpga(const composer::remote_ptr &dst, const void *host_addr);

    void copy_from_fpga(void *host_addr, const composer::remote_ptr &src);

    void free(composer::remote_ptr fpga_addr);

  private:
    cmd_server_file *cmd_server;
    int csfd;
    data_server_file *data_server;
    int dsfd;
    std::map<uint64_t, std::tuple<int, void *, int, std::string> > device2virtual;
  };
}

#endif //COMPOSER_FPGA_HANDLE_SIM_H
