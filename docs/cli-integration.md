# CLI integration contract

This document specifies the contract between the (future) `beethoven` CLI
and the `Beethoven-Software` build system. The CLI lives in the
`Beethoven-Hardware` repo (`src/main/scala/beethoven/cli/`); this file is
the SW-side spec it implements against.

After Phase 1–3 of the SW refactor, building a Beethoven project is two
cmake invocations and an sbt run, all of which the CLI orchestrates.
Users do not invoke `cmake` or `sbt` directly.

## 1. Install layout — what the CLI can rely on

After `cmake --install` of `Beethoven-Software`, these paths exist under
`${CMAKE_INSTALL_PREFIX}` (default `~/.local`):

```
${CMAKE_INSTALL_PREFIX}/
├── lib/                                            # or lib64 on RH-style
│   ├── libbeethoven-discrete.so
│   ├── libbeethoven-zynq.so
│   ├── libbeethoven_baremetal.a                    # only if baremetal built
│   └── cmake/
│       ├── beethoven/
│       │   ├── beethovenConfig.cmake
│       │   ├── beethovenConfigVersion.cmake
│       │   ├── beethoven-discrete-targets.cmake
│       │   └── beethoven-zynq-targets.cmake
│       └── beethoven_baremetal/
│           ├── beethoven_baremetalConfig.cmake
│           └── ...
├── include/beethoven/                              # public headers
└── share/beethoven_baremetal/arm-none-eabi.cmake   # baremetal toolchain
```

A breadcrumb is also written to the CMake user package registry:

```
~/.cmake/packages/beethoven/<md5(prefix)>           # contains the cmake/beethoven/ path
~/.cmake/packages/beethoven_baremetal/<md5(prefix)>
```

This means `find_package(beethoven)` works from any project with no
`CMAKE_PREFIX_PATH` or env vars set.

## 2. `~/.config/beethoven/config.toml` schema

The CLI maintains this file. It is the single source of truth for where
Beethoven is installed and where the source repos live (for the runtime
cmake project, which is per-project).

```toml
[install]
# Where libbeethoven was installed via cmake --install
prefix = "/home/mason/.local"
# Which platform components the install actually has libraries for
installed-components = ["discrete", "zynq"]
# Beethoven-Software version installed (from PROJECT_VERSION)
version = "0.0.0"

[source]
# Where Beethoven-Software source lives — the runtime cmake project
# is invoked here per-design. Need not equal $prefix.
beethoven-software = "/home/mason/.local/share/beethoven/src/Beethoven-Software"
beethoven-hardware = "/home/mason/.local/share/beethoven/src/Beethoven-Hardware"
```

## 3. `Beethoven.toml` (per-project) — fields the CLI consumes

```toml
[project]
name        = "..."          # required
version     = "0.0.0"        # default "0.0.0"

[hardware.beethoven-hardware]
path        = "..."          # OR
version     = "..."          # exactly one of path / version

# [software.beethoven-software]
# path = "..."               # optional dev-mode override

[platform]
target      = "aupzu3"       # one of: simulation | kria | kria2 | aupzu3 |
                             #          aws-f1 | aws-f2 | u200
build-mode  = "synthesis"    # one of: synthesis | simulation

[platform.<target>]
# target-specific params (e.g., dram-size-gb for aupzu3)

[build]
output-dir  = "target"       # default "target"

[runtime]
simulator   = "verilator"    # only meaningful when build-mode = simulation
                             # one of: verilator | icarus | vcs (default verilator)
```

Frontend selection is **not** a user knob: it is inferred from the platform
(`baremetal` → `chipkit`; everything else → `axi`).

## 4. `beethoven build` flow

```
beethoven build                                 (cwd = project root)
  │
  1. Load Beethoven.toml + ~/.config/beethoven/config.toml
  │    - if config.toml missing → run `beethoven init` (clone + install)
  │
  2. Verify version compatibility
  │    - read installed version from
  │      ${prefix}/lib/cmake/beethoven/beethovenConfigVersion.cmake
  │    - compare with the version Beethoven-Hardware was built against
  │    - on mismatch: error, suggest `beethoven update`
  │
  3. sbt run                                    → target/binding/, target/<mode>/hw/
  │    - main class is beethoven.cli.Run; toml feeds it via env/sys-props
  │
  4. cmake on Beethoven-Software/runtime        → target/<mode>/runtime/BeethovenRuntime
  │    - cmake -S ${beethoven-software}/runtime
  │           -B target/<mode>/runtime/_cmake
  │           -DBEETHOVEN_PROJECT_ROOT=$PWD
  │           -DBEETHOVEN_BUILD_MODE=<mode>
  │           -DBEETHOVEN_PLATFORM=<platform>
  │           -DBEETHOVEN_SIMULATOR=<simulator>   (simulation only)
  │    - cmake --build ... -j
  │
  5. cmake on <project>/sw                      → target/sw/<testbench>
  │    - cmake -S sw -B target/sw
  │           -DBEETHOVEN_PROJECT_ROOT=$PWD
  │           -DBEETHOVEN_PLATFORM=<platform>
  │    - No -DBEETHOVEN_BUILD_MODE — the testbench is mode-agnostic
  │      (libbeethoven's IPC layer is what it talks to; sim and synth
  │      daemons present the same shmem interface).
  │    - <project>/sw/CMakeLists.txt is just:
  │           find_package(beethoven REQUIRED)
  │           beethoven_build(my_tb SOURCES my_tb.cc)
  │      The platform component is auto-loaded by beethoven_build()
  │      based on -DBEETHOVEN_PLATFORM — no COMPONENTS needed.
  │    - links libbeethoven only — NOT runtime (separate processes)
```

The CLI's `<platform>` value maps the toml's `[platform] target` to the
corresponding libbeethoven component:

| `target`               | `BEETHOVEN_PLATFORM` |
|------------------------|----------------------|
| `simulation`           | `discrete`           |
| `aws-f1`, `aws-f2`     | `discrete`           |
| `u200`                 | `discrete`           |
| `kria`, `kria2`, `aupzu3` | `zynq`            |
| `baremetal`            | `baremetal`          |

## 5. `beethoven run` flow

```
beethoven run
  │
  1. Spawn the daemon
  │    target/<mode>/runtime/BeethovenRuntime           (verilator + synthesis)
  │  OR
  │    vvp -M target/<mode>/runtime
  │        -msim_BeethovenRuntime
  │        target/<mode>/runtime/beethoven.vvp          (icarus)
  │  OR
  │    target/<mode>/runtime/BeethovenTop               (vcs)
  │
  2. Wait for daemon to bind shmem channels
  │
  3. Exec target/sw/<testbench>
  │
  4. When testbench exits, signal daemon (target/<mode>/runtime/brt-kill)
```

Baremetal is the exception: there is no daemon. The user binary links
libbeethoven_baremetal directly and talks to the bus via mmio.

## 6. First-run bootstrap (`beethoven init`)

If `~/.config/beethoven/config.toml` is absent:

```
1. mkdir -p ~/.local/share/beethoven/src/
2. git clone Beethoven-Software → ~/.local/share/beethoven/src/Beethoven-Software
3. git clone Beethoven-Hardware → ~/.local/share/beethoven/src/Beethoven-Hardware
4. cmake -S .../Beethoven-Software \
         -B  .../Beethoven-Software/build \
         -DCMAKE_INSTALL_PREFIX=$HOME/.local
   cmake --build  .../Beethoven-Software/build -j
   cmake --install .../Beethoven-Software/build
5. write ~/.config/beethoven/config.toml
```

Step 4 is also what `beethoven update` performs (preceded by `git pull`).

## 7. Local-source override (developer mode)

If `Beethoven.toml` has either:

```toml
[hardware.beethoven-hardware]
path = "../Beethoven-Hardware"

[software.beethoven-software]
path = "../Beethoven-Software"
```

…the CLI uses those checkouts instead of the registered ones. For SW:

- The `cmake -S` for the runtime project points at the local checkout
- A build-tree-exported `find_package` is used:
  `cmake --build <local-sw>/build` first, then pass
  `-DCMAKE_PREFIX_PATH=<local-sw>/build` to all downstream cmake invocations
- The user's installed libbeethoven is shadowed for this project only

This is symmetric with how the HW side handles `[hardware.beethoven-hardware] path`.

## 8. Smoke test (post-CLI)

```bash
# Clean slate
rm -rf $HOME/.local/lib/cmake/beethoven*
rm -rf $HOME/.cmake/packages/beethoven*
rm -rf $HOME/.config/beethoven $HOME/.local/share/beethoven
rm -rf template/target

# First-run (triggers bootstrap)
cd template
beethoven build                    # clones, installs, builds

# Verify all artifacts
test -f $HOME/.local/lib/libbeethoven-discrete.so
test -f $HOME/.config/beethoven/config.toml
test -f target/binding/beethoven_hardware.h
test -f target/simulation/runtime/BeethovenRuntime
test -f target/sw/vector_tb

# Run end-to-end
beethoven run                      # daemon up, testbench passes, exits 0
```
