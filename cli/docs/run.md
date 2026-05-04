# `beethoven run`

Run the project end-to-end on real FPGA hardware. Auto-detects whether
a runtime daemon is already up for this project: if so, exec the
testbench against it; if not, launch a daemon, exec the testbench, and
tear the daemon down on exit.

## Synopsis

    beethoven run [TESTBENCH] [--no-build] [--no-launch] [--release]

## Description

The non-simulation analog of [`sim`](sim.md). Lifecycle:

1. **Validate target.** `Beethoven.toml` resolves to a real FPGA
   platform (`kria`, `aupzu3`, `aws-f1`, `u200`, …). Refuses if the
   target is `simulation`.
2. **Probe daemon.** Try a non-blocking shared `flock` on the
   project's lockfile (see [`runtime`](runtime.md) → *Concurrency*).
   - **Lock held → daemon up.** Skip launch; testbench will run
     against the existing daemon.
   - **Lock free → daemon down.** Plan to launch one (unless
     `--no-launch` is set).
3. **Build.** Auto-build whatever's stale (skip with `--no-build`):
   - Daemon up: rebuild only `sw`. The daemon's runtime artifacts are
     trusted; restart the daemon by hand if `hw`/`runtime` changed.
   - Daemon down: rebuild `runtime` and `sw`.
4. **Launch daemon (only if down).** Spawn `BeethovenRuntime` and
   wait until it has acquired the lock and posted ready. Track that
   we launched it.
5. **Launch testbench.** Exec the user binary; its `fpga_handle_t`
   connects to the daemon's shmem segment.
6. **Shutdown.** Wait for the testbench to exit. If `run` launched
   the daemon, send a clean shutdown and reap. If the daemon was
   already up before `run` started, leave it running. Propagate the
   testbench's exit code either way.

## Arguments

| Argument | Default | Meaning |
|---|---|---|
| `TESTBENCH` | the only testbench in `Beethoven.toml`, or error if multiple | Which testbench to run. |

## Options

| Flag | Meaning |
|---|---|
| `--no-build` | Skip step 3. |
| `--no-launch` | Refuse to launch a daemon; require one to be up. Useful for CI to detect orphaned setups. |
| `--release` | Run synth-mode artifacts (default for real hardware). |

## Inputs

- `<project>/Beethoven.toml`
- Built artifacts under `target/synth/` (auto-built unless `--no-build`).
- A bitstream loaded onto the FPGA (responsibility of `synth` + vendor flash tools).

## Outputs

- Console output from runtime and testbench.
- Exit code = the testbench's exit code.

## Errors

- Not in a project → exit `64`.
- Project target is `simulation` → exit `64`, suggests [`sim`](sim.md).
- `--no-launch` set but no daemon is up → exit `1`.
- FPGA not accessible (UIO device missing, AWS PCIe handle fails) → exit `1`.
- Build failure → exit `1`.

## Examples

    beethoven run
    beethoven run my_tb
    beethoven run --no-build
    beethoven run --no-launch        # CI: assert daemon is up
