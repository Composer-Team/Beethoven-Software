# `beethoven sim`

Run the project end-to-end in simulation. Auto-detects whether a
runtime daemon is already up for this project: if so, exec the
testbench against it; if not, launch the daemon (and the verilog
simulator), exec the testbench, and tear everything down on exit.

## Synopsis

    beethoven sim [TESTBENCH] [--no-build] [--no-launch] [--simulator <name>]

## Description

Lifecycle:

1. **Validate target.** `Beethoven.toml`'s `[platform].target` is anything
   except `baremetal` (which has no daemon). Build mode is forced to
   `simulation` regardless of which target it is — that's the whole point
   of the `sim` command.
2. **Probe daemon.** Try a non-blocking shared `flock` on the
   project's lockfile (see [`runtime`](runtime.md) → *Concurrency*).
   - **Lock held → daemon up.** Skip launch; testbench will run
     against the existing daemon (whatever simulator backend it was
     started with).
   - **Lock free → daemon down.** Plan to launch one (unless
     `--no-launch` is set).
3. **Build.** Auto-build whatever's stale (skip with `--no-build`):
   - Daemon up: rebuild only `sw`. The daemon's runtime/simulator are
     trusted; restart by hand if `hw`/`runtime` changed.
   - Daemon down: rebuild `hw`, `runtime`, and `sw`.
4. **Launch daemon + simulator (only if down).** Spawn
   `BeethovenRuntime` from `target/sim/runtime/`. For verilator the
   simulator is linked in (no separate process); for icarus/vcs spawn
   the simulator binary, which loads a VPI plugin connecting back
   over shmem. Track that we launched it.
5. **Launch testbench.** Exec the user binary
   (`target/sim/sw/<testbench>`).
6. **Shutdown.** Wait for the testbench to exit (or
   `handle.shutdown()`). If `sim` launched the daemon, tear down
   simulator and runtime cleanly. Otherwise leave them running.
   Propagate the testbench's exit code either way.

## Arguments

| Argument | Default | Meaning |
|---|---|---|
| `TESTBENCH` | the only testbench in `Beethoven.toml`, or error if multiple | Which testbench to run. |

## Options

| Flag | Meaning |
|---|---|
| `--no-build` | Skip step 3; assume artifacts are current. |
| `--no-launch` | Refuse to launch a daemon; require one to be up. |
| `--simulator <icarus\|verilator\|vcs>` | Override the simulator backend (default `icarus`, or `[platform].simulator` from `Beethoven.toml`). Only meaningful when launching a daemon — ignored if reusing an existing one. |

## Inputs

- `<project>/Beethoven.toml`
- Built artifacts under `target/sim/` (auto-built unless `--no-build`).

## Outputs

- Console output from runtime and testbench.
- Simulation traces under `target/simulation/`, if enabled in the design:
  `trace.vcd` / `trace.fst` (verilator/icarus), `BeethovenTrace.vpd` (vcs),
  plus DRAMsim3 logs (`dramsim3.txt`, `dramsim3log/`). The CLI pins the
  daemon's cwd to `target/simulation/` so these don't pollute the
  invocation directory; if the daemon is launched manually via
  `beethoven runtime run`, they land in *that* shell's cwd instead.
- Exit code = the testbench's exit code.

## Errors

- Not in a project → exit `64`.
- Project target is `baremetal` (no daemon) → exit `64`.
- `--no-launch` set but no daemon is up → exit `1`.
- Build failure → exit `1` with the underlying error.
- Runtime fails to start → exit `1`.
- Multiple testbenches and none specified → exit `2`.

## Examples

    beethoven sim                    # the only testbench
    beethoven sim my_tb              # specific testbench
    beethoven sim --no-build         # iterate fast: assume builds are current
    beethoven sim --no-launch        # require daemon already up
    beethoven sim --simulator verilator
