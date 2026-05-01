# Beethoven.toml reference

Full schema for the per-project `Beethoven.toml` manifest, including every
supported `[platform] target` and the keys each one accepts. Authoritative
sources: `Beethoven-Hardware/src/main/scala/beethoven/Manifest.scala`
(top-level parsing) and
`Beethoven-Hardware/src/main/scala/beethoven/Platforms/PlatformRegistry.scala`
(per-target parsing).

The `beethoven` CLI parses this file, generates `build.sbt` for sbt to run,
and orchestrates the cmake builds underneath.

## Top-level sections

```toml
[project]
name    = "vector_add"   # REQUIRED — appears in build outputs
version = "0.0.0"        # optional, default "0.0.0"

[hardware]
src-dir = "hw"           # optional, default "hw" — relative to manifest dir
                         # contains your Chisel/Scala accelerator sources

[hardware.beethoven-hardware]
# Provide EXACTLY ONE of `path` or `version`. `path` wins if both are set.
path    = "../Beethoven-Hardware"   # local checkout (developer mode)
# version = "0.1.6"                  # maven-resolved release

[software]
src-dir = "sw"           # optional, default "sw" — your testbench / host code

[platform]
target     = "aupzu3"    # REQUIRED — see "Targets" below
build-mode = "simulation" # REQUIRED — "simulation" or "synthesis"

[platform.<target>]      # exactly ONE matching table; required keys depend on target
# … per-platform params, see below …

[build]
output-dir = "target"    # optional, default "target" — where target/ lives
```

## Targets

The valid values for `[platform] target` and the `[platform.<target>]` table
each one expects. `kind` indicates the SW-side category — drives which
`libbeethoven-*.so` the user binary links and how the runtime daemon talks
to "the FPGA":

- **sim** — runs against a Verilator/Icarus/VCS daemon.
  `BEETHOVEN_PLATFORM=discrete` always (the daemon mediates all data via
  shared memory, regardless of the underlying RTL params).
- **discrete-silicon** — real PCIe-attached FPGA card, daemon talks via
  `fpga_mgmt`. `BEETHOVEN_PLATFORM=discrete`.
- **zynq-silicon** — real Zynq SoC (FPGA fabric + CPU on one chip sharing
  DDR). `BEETHOVEN_PLATFORM=zynq` in synth, `discrete` in sim (the runtime
  daemon mediates allocations in sim — see `issues/verilator-widebus.md`
  caveat for `target = "simulation"` specifically with Verilator).

### `target = "simulation"`

Generic, sim-tuned harness — fastest for iterating on accelerator logic.
Backed by `SimulationPlatform`, which extends `KriaPlatform` but overrides
the bus widths to be wider (see `SimulationPlatform.scala`).

| Key | Type | Default | Description |
|---|---|---|---|
| `clock-rate-mhz` | int | 100 | Pretend FPGA clock rate; drives DRAMsim3 timing |

Constraints: `build-mode = "synthesis"` is rejected (no synth target for `simulation`).

```toml
[platform]
target     = "simulation"
build-mode = "simulation"

[platform.simulation]
# clock-rate-mhz = 100
```

### `target = "kria"`

Xilinx Kria KV260 (Zynq UltraScale+ MPSoC). Backed by `KriaPlatform`.

| Key | Type | Default | Description |
|---|---|---|---|
| `memory-channels` | int | 1 | DDR channel count |
| `clock-rate-mhz` | int | 100 | FPGA clock; affects timing closure in synth |

```toml
[platform]
target     = "kria"
build-mode = "synthesis"

[platform.kria]
# memory-channels = 1
# clock-rate-mhz  = 100
```

### `target = "kria2"`

Variant of Kria with `AXIFrontBusProtocolFastMem` for clock-crossing tests.
Same parameters as Kria.

| Key | Type | Default | Description |
|---|---|---|---|
| `memory-channels` | int | 1 |   |
| `clock-rate-mhz` | int | 100 |   |

### `target = "aupzu3"`

RealDigital AUP-ZU3 board (Zynq UltraScale+ XCZU3EG). Backed by `AUPZU3Platform`.

| Key | Type | Default | Description |
|---|---|---|---|
| `dram-size-gb` | int | **REQUIRED** | Either `4` or `8` — the constructor asserts this; runtime error otherwise |
| `memory-channels` | int | 1 |   |
| `clock-rate-mhz` | int | 100 |   |

```toml
[platform]
target     = "aupzu3"
build-mode = "synthesis"

[platform.aupzu3]
dram-size-gb = 8        # REQUIRED — must be 4 or 8
# memory-channels = 1
# clock-rate-mhz  = 100
```

### `target = "aws-f1"`

AWS F1 (PCIe-attached Xilinx UltraScale+ via Amazon's FPGA service).
Backed by `AWSF1Platform extends U200Platform with PlatformHasDMA`.

| Key | Type | Default | Description |
|---|---|---|---|
| `memory-channels` | int | **REQUIRED** | 1–4. Drives DDR controller count and physical interface layout |
| `clock-recipe` | string | `"A0"` | One of `"A0"` (125 MHz), `"A1"` (250 MHz), `"A2"` (15 MHz). Anything else is rejected at construction |

`build-mode = "synthesis"` triggers AWS-specific post-processing: shell wrap,
TCL src list, optional rsync to an EC2 F1 instance, and `aws-fpga` setup.
See `AWSF1Platform.scala:62–169`.

```toml
[platform]
target     = "aws-f1"
build-mode = "simulation"

[platform.aws-f1]
memory-channels = 1     # REQUIRED
# clock-recipe  = "A0"
```

### `target = "aws-f2"`

AWS F2 (the successor; remote-only synth flow). Backed by `AWSF2Platform`.

| Key | Type | Default | Description |
|---|---|---|---|
| `remote-username` | string | `"ubuntu"` | SSH login on the build instance |

`memory-channels` is **hardcoded to 1** in the platform constructor and
not exposed in the toml.

```toml
[platform]
target     = "aws-f2"
build-mode = "synthesis"

[platform.aws-f2]
# remote-username = "ubuntu"
```

## Build mode × target legality

| target / build-mode | `simulation` | `synthesis` |
|---|---|---|
| `simulation` | ✅ | ❌ (rejected — no real silicon target) |
| `kria` | ✅ | ✅ |
| `kria2` | ✅ | ✅ |
| `aupzu3` | ✅ | ✅ |
| `aws-f1` | ✅ | ✅ (triggers EC2 deploy prompt) |
| `aws-f2` | ✅ | ✅ (remote SSH-driven synth) |

For sim mode, the HW side still emits the **target's actual RTL parameters**
(real bus widths, AXI variant, memory layout) — that's the point of running
sim against a non-`simulation` target. You catch bus-protocol bugs that
only surface at the production widths. Use `target = "simulation"` for the
fastest iteration loop and a non-sim target's sim mode for pre-tape-out
verification.

## Caveat: Verilator + `target = "simulation"`

There's a still-open issue: building the runtime daemon for
`target = "simulation"` + `BEETHOVEN_SIMULATOR=verilator` fails to compile
because `SimulationPlatform`'s wider strb signals trip a template
specialization gap in `runtime/include/data_channel.h`. Workaround: use
**Icarus** with `target = "simulation"`, or use any non-sim target with
Verilator. See `issues/verilator-widebus.md` for the full diagnosis.

## Developer-mode override

Both repos have a `path` form so you can iterate against a local checkout
without publishing to the maven repo:

```toml
[hardware.beethoven-hardware]
path = "../Beethoven-Hardware"

# [software.beethoven-software]
# path = "../Beethoven-Software"   # symmetric, used by the CLI to point
                                   # the runtime cmake at a local checkout
```

When `path` is set, the resolution is relative to the manifest's directory.

## Full annotated example

```toml
# A working AUP-ZU3 simulation config — every key shown, defaults commented.

[project]
name    = "vector_add"
version = "0.0.1"                   # optional, default "0.0.0"

[hardware]
# src-dir = "hw"                    # optional, default "hw"

[hardware.beethoven-hardware]
path    = "../Beethoven-Hardware"   # OR `version = "X.Y.Z"`

# [software]                        # entire section optional
# src-dir = "sw"                    # default "sw"

[platform]
target     = "aupzu3"               # one of: simulation | kria | kria2
                                    #         aupzu3 | aws-f1 | aws-f2
build-mode = "simulation"           # "simulation" or "synthesis"

[platform.aupzu3]
dram-size-gb     = 8                # REQUIRED — 4 or 8 only
# memory-channels  = 1              # default 1
# clock-rate-mhz   = 100            # default 100

# [build]                           # entire section optional
# output-dir = "target"             # default "target"
```

## Path layout the CLI derives from this manifest

For the example above, the CLI generates:

```
<manifest-dir>/
└── target/                              # [build] output-dir
    ├── binding/                         # generated C bindings (mode-agnostic)
    ├── sw/                              # user testbench (mode-agnostic)
    ├── simulation/                      # [platform] build-mode = simulation
    │   ├── hw/                          # platform's RTL + harness
    │   └── runtime/BeethovenRuntime     # daemon
    └── .cache/                          # cross-mode shared
```

`folder-hierarchy.md` (top of the refurbish repo) has the full per-mode
tree.

## When parsing fails

Common errors and what they mean (see `Manifest.scala::err()` callers):

- `Beethoven.toml not found at <path>` — CLI couldn't find the manifest.
  The CLI does *not* walk up to find one; it expects `Beethoven.toml`
  in the cwd you invoke it from.
- `Missing required key project.name` — `[project] name` is required.
- `[hardware.beethoven-hardware] requires either path or version.` —
  exactly one of those keys must be present.
- `[platform] build-mode='X' is invalid.` — only `simulation` or
  `synthesis`.
- `Unknown [platform] target='X'.` — typo in the target name. Valid
  list above.
- `Missing required key [platform.aupzu3] dram-size-gb.` — required
  key for that platform.
- `[platform.aupzu3] dram-size-gb must be an integer; got String (...)` —
  toml type mismatch (e.g. you wrote `"8"` instead of `8`).
