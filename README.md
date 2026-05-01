# Beethoven Software

The host-side companion to `Beethoven-Hardware`. Provides:

- **libbeethoven** — the user-facing API (`fpga_handle_t`, allocator,
  command/response handles). Installed once into `${CMAKE_INSTALL_PREFIX}`,
  with platform-tagged variants (`libbeethoven-discrete.so`,
  `libbeethoven-zynq.so`) coexisting in a single install.
- **runtime** — the per-design `BeethovenRuntime` daemon that owns the
  simulator (Verilator/Icarus/VCS) or talks to the real FPGA. Built per
  project, never globally installed.
- **baremetal variant** — `libbeethoven_baremetal.a` for Cortex-M55
  testbenches, behind a separate cmake package.

## Dependencies

- cmake ≥ 3.20
- A C++17/20 compiler (gcc 9+ / clang 10+)
- For simulation: one of Verilator (≥ 5.0), Icarus Verilog, or VCS

```bash
# Debian/Ubuntu
sudo apt-get install cmake build-essential verilator iverilog

# RHEL / Fedora
sudo dnf install cmake gcc-c++ verilator iverilog

# macOS
brew install cmake verilator
```

## Install

No sudo needed. Default prefix is `~/.local`.

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j
cmake --install build
```

This produces:

```
~/.local/
├── lib/
│   ├── libbeethoven-discrete.so
│   ├── libbeethoven-zynq.so
│   └── cmake/beethoven/{beethovenConfig.cmake,beethoven-*-targets.cmake,...}
└── include/beethoven/{*.h,allocator/}
```

A breadcrumb is written to the CMake user package registry so downstream
`find_package(beethoven)` works with no env vars or `CMAKE_PREFIX_PATH`:

```
~/.cmake/packages/beethoven/<md5(prefix)>      # one-line text pointer
```

### Build only a subset of host platforms

```bash
cmake -S . -B build -DBEETHOVEN_PLATFORMS="zynq"      # zynq only
cmake -S . -B build -DBEETHOVEN_PLATFORMS="discrete;zynq"  # default
```

### Baremetal (Cortex-M55) variant

Separate package, separate toolchain. Off by default.

```bash
export M55_SRC=/path/to/cortex-m55/src
cmake -S . -B build-bm \
      -DBEETHOVEN_BAREMETAL=ON \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build-bm -j && cmake --install build-bm
```

## Per-project build (orchestrated by the `beethoven` CLI)

A user project never invokes this repo's cmake directly after installation.
The CLI (in `Beethoven-Hardware`) orchestrates two cmake projects per
build:

1. The **runtime** cmake project (`runtime/CMakeLists.txt`) — produces
   `<project>/target/<mode>/runtime/BeethovenRuntime`.
2. The user's **sw** cmake project (`<project>/sw/CMakeLists.txt`,
   trivially `find_package(beethoven REQUIRED) + beethoven_build(...)`) —
   produces `<project>/target/<mode>/sw/<testbench>`.

See [`docs/cli-integration.md`](docs/cli-integration.md) for the full
contract.

## Layout

```
Beethoven-Software/
├── CMakeLists.txt               # ~10-line dispatcher
├── include/beethoven/           # public headers
├── src/                         # libbeethoven sources
├── platforms/
│   ├── host/CMakeLists.txt      # discrete + zynq COMPONENTS loop
│   └── baremetal/CMakeLists.txt # Cortex-M55 variant
├── runtime/                     # per-project daemon — built by the CLI
│   ├── CMakeLists.txt
│   ├── DRAMsim3/                # vendored
│   └── src/{core,frontends/{axi,chipkit},fpga}/
├── cmake/                       # *.cmake.in templates, toolchain files
└── docs/cli-integration.md
```
