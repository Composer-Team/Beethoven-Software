# `beethoven build`

Build the project's three artifacts: hardware (verilog + bindings),
runtime daemon, and user testbench.

## Synopsis

    beethoven build [TARGET] [--release] [-j N]

Where `TARGET` is one of `hw`, `runtime`, `sw`, or omitted (= all).

## Description

`build` is sliced into three peers:

| Target | Tool | Produces | Reads from |
|---|---|---|---|
| `hw` | sbt | `<project>/target/binding/beethoven_hardware.cc`, generated verilog | `<project>/hw/` chisel sources, `Beethoven.toml` |
| `runtime` | cmake | `<project>/target/<mode>/runtime/BeethovenRuntime` | `${BEETHOVEN_RUNTIME_SRC_DIR}` + the `hw` artifacts |
| `sw` | cmake | `<project>/target/sw/<testbench>` | `<project>/sw/` + libbeethoven + the `hw` bindings |

Bare `build` runs all three in dependency order. Each subgroup is
**strict** — it rejects rather than implicitly building prereqs:

- `build runtime` rejects if `target/binding/beethoven_hardware.cc` is
  missing (suggests `beethoven build hw`) or if libbeethoven isn't
  installed (suggests [`setup`](setup.md)).
- `build sw` rejects if libbeethoven isn't installed or if bindings
  are missing (the `beethoven_build()` cmake macro pulls the binding
  `.cc` in as a source).

For the forgiving "build whatever's stale and run" behavior, use
[`sim`](sim.md) or [`run`](run.md) — they auto-build prereqs.

This split matters for cross-machine workflows: `build hw` can run on
an EDA workstation while `build sw` runs on a Zynq ARM64 host, with
the binding file shipped between them.

## Options

| Flag | Meaning |
|---|---|
| `--release` | Build for synthesis (`target/synthesis/runtime/`); default is simulation (`target/simulation/runtime/`). Affects `runtime` only — `hw` and `sw` are mode-agnostic. |
| `-j N` | Parallel jobs for cmake/make. Defaults to all cores. |

## Inputs

- `<project>/Beethoven.toml` — for project name, target, mode.
- `<project>/hw/`, `<project>/sw/` — sources.
- `${BEETHOVEN_RUNTIME_SRC_DIR}` — exported by `beethovenConfig.cmake`.

## Outputs

Layout under `<project>/target/`:

    target/
    ├── binding/                       # shared (chisel-generated bindings)
    │   └── beethoven_hardware.cc      # produced by `build hw`
    ├── sw/                            # mode-agnostic; only platform affects link
    │   └── <testbench>                # produced by `build sw`
    ├── simulation/
    │   └── runtime/
    │       └── BeethovenRuntime       # produced by `build runtime`
    └── synthesis/
        └── runtime/
            └── BeethovenRuntime       # produced by `build runtime --release`

`sw` is mode-agnostic because the user binary only links libbeethoven
and the binding `.cc` — neither depends on whether the daemon is
sim-backed or silicon-backed at build time.

## Errors

- Not in a project → exit `64`.
- `build runtime` without bindings → exit `64`.
- `build runtime` without libbeethoven → exit `64`.
- `build sw` without bindings → exit `64`.
- `build sw` without libbeethoven installed → exit `64`.
- Compilation failure → exit `1` with the underlying error.

## Examples

    beethoven build                    # all three, simulation mode
    beethoven build --release          # all three, synthesis mode
    beethoven build hw                 # just regenerate verilog/bindings
    beethoven build runtime            # rebuild daemon
    beethoven build sw                 # rebuild testbench (fastest iteration)
