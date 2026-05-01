# Beethoven Software

The host-side companion to [`Beethoven-Hardware`](https://github.com/Composer-Team/Beethoven-Hardware).
Three deliverables out of one repo:

- **libbeethoven** — the user-facing C++ API (`fpga_handle_t`, allocator,
  command/response handles). Installed once globally; per-platform
  variants (`libbeethoven-discrete.so`, `libbeethoven-zynq.so`) coexist
  in a single install.
- **BeethovenRuntime** — the per-design daemon that owns the simulator
  (Verilator / Icarus / VCS) or talks to a real FPGA over UIO/PCIe. Built
  *per project*, never globally installed (the daemon is design-coupled).
- **libbeethoven_baremetal** — static library for Cortex-M55 testbenches.
  Separate cmake package, separate toolchain, no daemon.

End users do not run this repo's cmake directly after the one-time install
— the `beethoven` CLI orchestrates per-project builds. See
[`docs/cli-integration.md`](docs/cli-integration.md) for the contract.

## Dependencies

- **cmake ≥ 3.20**
- A C++17/20 compiler (gcc 9+ / clang 10+)
- For simulation: one of **Verilator ≥ 5.0**, **Icarus Verilog**, or **VCS**

```bash
# Debian/Ubuntu
sudo apt-get install cmake build-essential verilator iverilog

# RHEL/Fedora
sudo dnf install cmake gcc-c++ verilator iverilog

# macOS
brew install cmake verilator
```

VCS is licensed; install per Synopsys docs and ensure `vcs` is on `PATH`.

## Install (no sudo)

Default prefix is `~/.local`. The CMake user package registry takes care
of discovery — downstream `find_package(beethoven)` works with no env
vars and no `CMAKE_PREFIX_PATH`.

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build
```

The exact file layout depends on your distro's `GNUInstallDirs`
convention (`lib/` on Debian/macOS, `lib64/` on RHEL/Fedora):

```
~/.local/
├── lib[64]/
│   ├── libbeethoven-discrete.so
│   ├── libbeethoven-zynq.so
│   └── cmake/beethoven/
│       ├── beethovenConfig.cmake
│       ├── beethovenConfigVersion.cmake
│       ├── beethoven-discrete-targets.cmake
│       ├── beethoven-discrete-targets-release.cmake
│       ├── beethoven-zynq-targets.cmake
│       └── beethoven-zynq-targets-release.cmake
└── include/beethoven/
    ├── allocator/{alloc,alloc_baremetal,device_allocator}.h
    ├── arm_cache.h
    ├── beethoven_consts.h
    ├── fpga_handle.h
    ├── response_handle.h
    ├── rocc_cmd.h
    ├── rocc_response.h
    ├── runtime_ipc.h
    └── util.h
```

Plus a one-line registry breadcrumb (zero-config discovery):

```
~/.cmake/packages/beethoven/<md5(prefix)>      # contains the cmake config dir
```

### Verify the install

```bash
mkdir /tmp/find-test && cd /tmp/find-test
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(find-test LANGUAGES CXX)
find_package(beethoven 0.0.0 REQUIRED COMPONENTS discrete zynq)
get_target_property(_loc APEX::beethoven-discrete IMPORTED_LOCATION_RELEASE)
message(STATUS "Found: ${_loc}")
EOF
cmake -B build
# Expect: -- Found: /home/<you>/.local/lib[64]/libbeethoven-discrete.so
```

### Subset of host platforms

Default builds both `discrete` and `zynq`. To build only one:

```bash
cmake -S . -B build -DBEETHOVEN_PLATFORMS="zynq" -DCMAKE_INSTALL_PREFIX=$HOME/.local
```

### Baremetal (Cortex-M55) variant

Separate package, separate toolchain. Off by default — the host build
above doesn't produce it.

```bash
export M55_SRC=/path/to/cortex-m55/src
cmake -S . -B build-bm \
      -DBEETHOVEN_BAREMETAL=ON \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build-bm -j && cmake --install build-bm
```

Produces `libbeethoven_baremetal.a` and a separate `find_package`
package: `find_package(beethoven_baremetal REQUIRED)`.

### Update / reinstall

`cmake --install build` is idempotent — overwriting an existing install
is fine. To do a clean rebuild after a `git pull`:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j
cmake --install build
```

The `SameMinorVersion` compatibility check in
`beethovenConfigVersion.cmake` will fail downstream builds if the
installed version's minor differs from what they require — bump
accordingly.

## Per-project build flow (CLI-orchestrated)

A user project's `Beethoven.toml` drives two cmake projects per build,
both invoked by the `beethoven` CLI:

1. **runtime** — `cmake -S Beethoven-Software/runtime -B
   <project>/target/<mode>/runtime/_cmake -DBEETHOVEN_PROJECT_ROOT=...
   -DBEETHOVEN_BUILD_MODE=... -DBEETHOVEN_PLATFORM=...
   -DBEETHOVEN_SIMULATOR=...`
   Produces `<project>/target/<mode>/runtime/BeethovenRuntime`.
   Testbenches signal shutdown via `handle.shutdown()` from libbeethoven.
2. **user sw** — `cmake -S <project>/sw -B
   <project>/target/<mode>/sw -DBEETHOVEN_PROJECT_ROOT=...
   -DBEETHOVEN_PLATFORM=...`
   The user's `sw/CMakeLists.txt` is trivially:

   ```cmake
   find_package(beethoven REQUIRED COMPONENTS discrete)
   beethoven_build(my_tb SOURCES my_tb.cc)
   ```

   `beethoven_build()` is provided by the package config; it links
   `APEX::beethoven-${BEETHOVEN_PLATFORM}` and pulls in
   `<project>/target/binding/beethoven_hardware.cc` (the design's
   generated bindings, produced by sbt earlier in the flow).

The runtime daemon and the user testbench are **separate processes**
that talk over POSIX shared memory. The user testbench links
libbeethoven only — never the runtime.

`docs/cli-integration.md` has the full schema, env contract, and CLI
flow.

## Layout

```
Beethoven-Software/
├── CMakeLists.txt                          # ~10-line dispatcher
├── include/beethoven/                      # public headers
├── src/                                    # libbeethoven sources
├── platforms/
│   ├── host/CMakeLists.txt                 # discrete + zynq COMPONENTS loop
│   └── baremetal/CMakeLists.txt            # Cortex-M55 variant
├── cmake/
│   ├── beethovenConfig.cmake.in            # find_package template
│   ├── beethoven_baremetalConfig.cmake.in
│   └── arm-none-eabi.cmake                 # baremetal toolchain
├── runtime/                                # per-project daemon
│   ├── CMakeLists.txt                      # CLI-invoked
│   ├── DRAMsim3/                           # vendored
│   ├── include/                            # internal daemon headers
│   ├── src/
│   │   ├── core/                           # cmd_server, data_server, mmio,
│   │   │                                   # tick, mem_ctrl, DataWrapper, fpga_main
│   │   ├── frontends/
│   │   │   ├── axi/                        # default frontend
│   │   │   └── chipkit/                    # baremetal frontend
│   │   └── fpga/                           # synth-mode glue
│   └── scripts/                            # tab.tab (VCS), kria_alloc_pages.py
├── test/                                   # libbeethoven host tests
└── docs/cli-integration.md                 # CLI <-> SW build contract
```

## Troubleshooting

**`find_package(beethoven)` finds an old install.** CMake searches the
user registry before `CMAKE_SYSTEM_PREFIX_PATH`, but if `/usr/local/`
contains a higher-version stale install (with `AnyNewerVersion` compat),
it can win. Either uninstall it (`rm -rf /usr/local/lib*/cmake/beethoven
/usr/local/include/beethoven /usr/local/lib*/libbeethoven*.so`) or
override with `-DCMAKE_PREFIX_PATH=$HOME/.local`.

**`Could not find beethoven (missing: discrete)`.** You requested a
component that wasn't built. Re-install with `BEETHOVEN_PLATFORMS`
including the platform you need.

**Runtime cmake fails with `Cannot find generated binding`.** The runtime
project depends on `<project>/target/binding/beethoven_hardware.cc`,
produced by `sbt run` on the project. Run sbt first (or let the CLI
do it).

## License

MIT — see `LICENSE.md`.
