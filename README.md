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

The simplest path is the wrapper script:

```bash
./install.sh                       # ~/.local, Release
./install.sh --prefix /opt/foo     # different prefix
./install.sh --debug               # -O0 -g
./install.sh --clean               # wipe build/ first
./install.sh --help
```

Or the cmake commands directly:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build
```

To remove an install: `./uninstall.sh` (lists what would be removed,
asks for confirmation; `--yes` skips the prompt, `--dry-run` skips the
removal). It removes the libs, headers, cmake configs, the runtime
source-package, and the matching cmake user-package-registry entry,
but only entries that point inside the prefix you pass.

The exact file layout depends on your distro's `GNUInstallDirs`
convention (`lib/` on Debian/macOS, `lib64/` on RHEL/Fedora):

```
~/.local/
├── lib[64]/
│   ├── libbeethoven-discrete.so                   # built artifact
│   ├── libbeethoven-zynq.so                       # built artifact
│   └── cmake/beethoven/
│       ├── beethovenConfig.cmake
│       ├── beethovenConfigVersion.cmake
│       ├── BeethovenBuildHelpers.cmake            # internal: shared by both Configs
│       ├── beethoven-discrete-targets.cmake
│       ├── beethoven-discrete-targets-release.cmake
│       ├── beethoven-zynq-targets.cmake
│       └── beethoven-zynq-targets-release.cmake
├── include/beethoven/                              # public headers
│   ├── allocator/{alloc,alloc_baremetal,device_allocator}.h
│   ├── arm_cache.h
│   ├── beethoven_consts.h
│   ├── fpga_handle.h
│   ├── response_handle.h
│   ├── rocc_cmd.h
│   ├── rocc_response.h
│   ├── runtime_ipc.h
│   └── util.h
└── share/beethoven/runtime-src/                    # runtime cmake project, source-package
    ├── CMakeLists.txt                                (cmake'd per-project by the CLI)
    ├── DRAMsim3/                                     (vendored DRAM model, full source)
    ├── include/{core,frontends/{axi,chipkit},fpga}/
    ├── src/{core,frontends/{axi,chipkit},fpga}/
    ├── scripts/{tab.tab, kria_alloc_pages.py}
    └── verilog_resources/BUFG.v
```

Two convenience pointers are also written:

```
~/.cmake/packages/beethoven/<md5(prefix)>      # registry breadcrumb;
                                                 makes find_package(beethoven) zero-config
```

…and `beethovenConfig.cmake` exports `BEETHOVEN_RUNTIME_SRC_DIR`
(absolute path to `share/beethoven/runtime-src/`) so consumers reading
the package config — the CLI in particular — can find the runtime
cmake project without hardcoding paths.

### What's installed vs cached vs not installed

- **Installed (copied to `~/.local`):** `libbeethoven-*.so`, public
  headers, cmake configs, the runtime cmake project. After
  `cmake --install build`, you can `rm -rf` your dev clone and a
  downstream `beethoven build` still works.
- **Per-project (built by the CLI, not installed):** the
  `BeethovenRuntime` daemon binary itself, lands at
  `<your-project>/target/<mode>/runtime/BeethovenRuntime`. The runtime
  is design-coupled (templated on the user's verilator output), so
  it's never globally cached.

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

`install.sh` always builds both `discrete` and `zynq`. To build only one,
drop down to plain cmake:

```bash
cmake -S . -B build -DBEETHOVEN_PLATFORMS="zynq" -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j && cmake --install build
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

1. **runtime** — `cmake -S ${BEETHOVEN_RUNTIME_SRC_DIR} -B
   <project>/target/<mode>/runtime/_cmake -DBEETHOVEN_PROJECT_ROOT=...
   -DBEETHOVEN_BUILD_MODE=... -DBEETHOVEN_PLATFORM=...
   -DBEETHOVEN_SIMULATOR=...`
   `BEETHOVEN_RUNTIME_SRC_DIR` (= `~/.local/share/beethoven/runtime-src`
   by default) is exported by `beethovenConfig.cmake`, so the CLI
   discovers it via `find_package(beethoven)` — no hardcoded paths and
   no dependency on the dev clone. Produces
   `<project>/target/<mode>/runtime/BeethovenRuntime`. Testbenches
   signal shutdown via `handle.shutdown()` from libbeethoven.
2. **user sw** — `cmake -S <project>/sw -B
   <project>/target/sw -DBEETHOVEN_PROJECT_ROOT=...
   -DBEETHOVEN_PLATFORM=...`
   The user's `sw/CMakeLists.txt` is trivially:

   ```cmake
   find_package(beethoven REQUIRED)
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

## Repo layout

```
Beethoven-Software/
├── CMakeLists.txt                          # ~5-line dispatcher: add_subdirectory(libbeethoven)
├── install.sh / uninstall.sh               # convenience wrappers around cmake
│
├── libbeethoven/                           # the installed library
│   ├── CMakeLists.txt                      #   selects host vs baremetal subtree
│   ├── include/beethoven/                  #   public headers
│   ├── src/                                #   libbeethoven sources
│   ├── platforms/
│   │   ├── host/CMakeLists.txt             #   discrete + zynq COMPONENTS loop
│   │   └── baremetal/CMakeLists.txt        #   Cortex-M55 variant
│   ├── cmake/
│   │   ├── beethovenConfig.cmake.in
│   │   ├── beethoven_baremetalConfig.cmake.in
│   │   ├── BeethovenBuildHelpers.cmake     #   shared body of beethoven_build()
│   │   └── arm-none-eabi.cmake             #   baremetal toolchain
│   └── test/                               #   libbeethoven host tests
│
├── runtime/                                # per-project daemon (cmake project,
│   ├── CMakeLists.txt                      #   installed under share/ as a
│   ├── DRAMsim3/                           #   source package; cmake'd per
│   ├── custom_dram_configs/                #   project by the CLI)
│   ├── include/{core,frontends/{axi,chipkit},fpga}/
│   ├── src/{core,frontends/{axi,chipkit},fpga}/
│   ├── scripts/                            #   tab.tab (VCS), kria_alloc_pages.py
│   └── verilog_resources/BUFG.v
│
└── docs/
    ├── cli-integration.md                  # CLI <-> SW build contract
    ├── beethoven-toml-reference.md         # full Beethoven.toml schema
    └── issues/verilator-widebus.md         # one open template-specialization gap
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
