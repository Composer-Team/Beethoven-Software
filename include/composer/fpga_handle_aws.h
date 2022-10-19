//
// Created by Chris Kjellqvist on 10/19/22.
//

#ifndef COMPOSER_FPGA_HANDLE_AWS_H
#define COMPOSER_FPGA_HANDLE_AWS_H

#ifdef USE_AWS
class fpga_handle_real_t : public fpga_handle_t {
  public:
    explicit fpga_handle_real_t(int id);

    ~fpga_handle_real_t();

    void write(size_t addr, uint32_t data) const override;

    uint32_t read(size_t addr) const override;

    uint32_t is_write_ready() const override;

    void fpga_shutdown() const override;

    int get_write_fd() const override;

    int get_read_fd() const override;

    bool is_real() const override;

  private:
    //    int rc;
    int slot_id = -1;
    pci_bar_handle_t pci_bar_handle = -1;
    int xdma_write_fd = -1;
    int xdma_read_fd = -1;
  };
#endif

#endif //COMPOSER_FPGA_HANDLE_AWS_H
