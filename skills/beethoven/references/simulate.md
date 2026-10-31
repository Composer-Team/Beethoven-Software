# Creating projects and simulating

Every command that may run sbt (`new` does not; `check`, `build`, `build hw`,
`sim` do) needs SDKMAN sourced in the same shell command:
`source "$HOME/.sdkman/bin/sdkman-init.sh" && beethoven ...`.

## Create a project

```bash
beethoven new my-design                        # pure-Chisel accelerator, target = default
beethoven new my-design --verilog              # Verilog-blackbox top (<Accel>Core.v)
beethoven new my-design --platform aupzu3      # pick the deployment target up front
beethoven init [--platform ..] [--verilog]     # scaffold into the current dir instead
```

Other flags: `--accel <ClassName>` (default: PascalCased name), `--vcs`
(`git init` + initial commit). Names must start with a letter and contain only
letters, digits, `-`, `_`. `new` refuses an existing directory; `init` refuses
if `Beethoven.toml` exists.

With `--verilog`, the hardware is a hand-written `hw/<Accel>Core.v`; the
framework re-syncs its port list against the Scala command struct
(`<Accel>Cmd.scala`) on every build.

Layout:

```
my-design/
├── Beethoven.toml        # the manifest — edit this (see manifest.md)
├── build.sbt             # generated, do not edit
├── hw/                   # Chisel/Scala (and .v for --verilog)
├── sw/                   # C++ testbench + CMakeLists.txt (find_package(beethoven) + beethoven_build)
└── target/               # all generated output
    ├── binding/          # beethoven_hardware.{h,cc}: C++ bindings to the accelerator commands
    ├── sw/               # compiled testbench(es)
    ├── simulation/{hw,runtime}/
    └── synthesis/{hw,runtime,implementation,aws}/
```

Ready-made examples live in https://github.com/Composer-Team/Beethoven-Zoo
(one project per folder: DotProduct is the simplest; SystolicArray, SHAKE256,
A3, GameBoy). `cd` into one and `beethoven build && timeout 600 beethoven sim`.

## Fast checks before a full build

```bash
beethoven check     # parse Beethoven.toml, check tools on PATH, elaborate Chisel (no codegen)
beethoven info      # resolved target/platform/prefix; `--format json` for parsing
```

## Build and simulate

```bash
beethoven build                       # hw + runtime + sw (sim and synth passes on synth-capable targets)
beethoven build --simulation          # sim pass only — faster on kria/aupzu3/aws-f2 projects
timeout 600 beethoven sim             # run the only testbench
timeout 600 beethoven sim my_tb -- --n 1024   # choose a testbench, forward args
```

`sim` flags: `--no-build` (run what exists), `--no-launch` (require an
already-running daemon), `--simulator icarus|verilator|vcs` (override the
manifest for this launch only).

Rebuild rules (important): `sim` builds only what is missing.
After editing `sw/` → `beethoven build sw`. After editing `hw/` or
`Beethoven.toml` → `beethoven build` (or at least `build hw`; `sim` always
re-runs the incremental runtime build when it launches a daemon).

`sim` works for every target except `baremetal`. For a real-FPGA target
(e.g. `aupzu3`), simulation uses that platform's real bus widths and memory
layout, which catches integration bugs before synthesis; `default` is the
fastest for iterating on logic.

## Simulator choice

`[platform] simulator = "icarus"` (default) or `"verilator"` (faster, needs
Verilator ≥ 5). Known incompatibilities are in troubleshooting.md; if one
backend fails to compile or elaborate, try the other before editing RTL.

## Outputs

The daemon runs with cwd `target/simulation/`, so waveforms
(`dump.vcd` / `trace.vcd` / `trace.fst`) and DRAMsim3 logs land there.
Icarus builds always trace, so the VCD grows for the whole run.

## Manual daemon (debugging, many quick sw iterations)

`sim` normally starts and stops the daemon itself. To keep one alive:

```bash
# start in the background (agent: use a background shell); it never exits on its own
cd target/simulation && beethoven runtime run --log-file runtime.log
# then, from the project root, iterate:
beethoven build sw && timeout 300 beethoven sim --no-launch
```

While a daemon is up, `sim` rebuilds only `sw` and never restarts the daemon,
so hardware changes need a daemon restart. Stop it by terminating that
background process (SIGINT/SIGTERM). `beethoven runtime kill` is the
heavy-handed reset: SIGKILL + delete lockfile, shared memory, and
`target/<mode>/runtime/`.

## When a sim hangs or fails

1. It was under `timeout`, so it stopped. Check whether a daemon is left over
   (`beethoven sim --no-launch` errors if none is up); if so, terminate it.
2. Delete the oversized trace: `rm -f target/simulation/*.vcd`.
3. Typical cause: the core never asserts `resp.valid`, or a memory reader/
   writer never completes. Read final signal values from the VCD without a
   viewer: find signal ids in the header (`$var wire <w> <id> <name> $end`),
   then the last value change for each id (`^[01]<id>$` or `^b[01]+ <id>$`).
4. See troubleshooting.md for known simulator issues.
