# Composer-Software

This repository contains the necessary software to interact with a Composer design over the AWS F1 FPGA Framework.

### Dependencies

[Amazon F1 SDK](https://github.com/aws/aws-fpga) - this is a dependency from the 
[Composer Hardware](https://github.com/ChrisKjellqvist/Composer-Hardware) repository. Under normal
circumstances, the Composer-Hardware directory will contain the SDK, which can be used instead of installation in
another directory. For everything here to work properly, `SDK_DIR` needs to be defined, which is typically set when
running `sdk_setup.sh` from the SDK directory. Do **not** set this manually.

If you are 

### Installation

```cmake -B build -S  . && cd build && make install```

This requires root permissions. For user-mode builds make sure to pass the `-DCMAKE_INSTALL_PREFIX=<dir>` to cmake.
After which, make sure that this path is accessible to the linker and cmake.