# `beethoven runtime`

Manage the per-project runtime daemon — its build, its lifecycle, and
its artifacts.

## Synopsis

    beethoven runtime <build|run|clean> [OPTIONS]

Bare `beethoven runtime` prints help.

## Subcommands

### `runtime build`

Identical to [`build runtime`](build.md). Cmake-builds the daemon for
the current build mode (sim by default, synth with `--release`).
Strict — rejects without `target/binding/beethoven_hardware.cc`.

### `runtime run`

Launches the built daemon in the **foreground**. The daemon owns the
simulator (verilator built-in, or icarus/vcs as a separate process)
and the shmem IPC channel that the user testbench connects to.

At startup the daemon takes an exclusive `flock` on the project's
lockfile (see *Concurrency* below). If the lock is already held by
another `BeethovenRuntime` for this project, the daemon refuses to
start and prints the holding PID — stop the existing instance first.

Stop with **Ctrl+C** — there is no `runtime stop`. The kernel
releases the lock automatically when the process exits, even on
`SIGKILL`. Orphaned daemons can be killed with `pkill BeethovenRuntime`
(the lockfile contains the PID for the targeted case
`kill -9 <pid>`).

While `runtime run` is up, [`run`](run.md) and [`sim`](sim.md) will
auto-detect the running daemon and exec the testbench against it
without tearing it down.

### `runtime clean`

Removes runtime artifacts under `target/<mode>/runtime/`. Does **not**
touch `target/binding/` or `target/<mode>/sw/`. For a full project
clean, see [`clean`](clean.md).

## Options

`runtime build` and `runtime clean` accept `--release` to operate on
`target/synth/runtime/` instead of `target/sim/runtime/`.

`runtime run` accepts:

| Flag | Meaning |
|---|---|
| `--release` | Run the synth-mode daemon. |
| `--log-file <path>` | Tee daemon stderr to a file. |

## Concurrency

At most one `BeethovenRuntime` may run per project at a time. This is
enforced by an exclusive `flock` on a per-project lockfile.

| | |
|---|---|
| **Lockfile path** | `<run-dir>/beethoven-<uid>-<key>.lock`, where `<run-dir>` is the first of `$XDG_RUNTIME_DIR` (Linux), `$TMPDIR` (macOS, per-user `/var/folders/...`), or `/tmp` (universal fallback). Flat filename — no subdirectory created. |
| **Project key** | MD5 of the canonicalized absolute project root. The CLI and the daemon compute it independently and arrive at the same path. |
| **Lockfile contents** | Daemon's PID, written after the lock is acquired. Used for diagnostics only. |
| **Crash safety** | Lock is held by the kernel, not by file contents — `SIGKILL`/segfault/OOM all release the lock automatically. The daemon also unlinks any stale shmem segment on startup, after acquiring the lock, so the next instance starts clean. |

The CLI probes daemon presence with a non-blocking shared `flock`:
success means no daemon is up, `EWOULDBLOCK` means one is.

## Inputs

- `<project>/Beethoven.toml`
- For `run`: a previously built `BeethovenRuntime` binary.

## Outputs

- `runtime build`: see [`build`](build.md).
- `runtime run`: foreground process; logs on stderr.
- `runtime clean`: files removed under `target/<mode>/runtime/`.

## Errors

- Not in a project → exit `64`.
- `runtime run` without a built daemon → exit `64`, suggests
  `beethoven runtime build`.
- `runtime run` while another instance holds the lock for this
  project → exit `1`. The daemon prints the holding PID.

## Examples

    beethoven runtime build
    beethoven runtime run                        # foreground, Ctrl+C to stop
    beethoven runtime run --release
    beethoven runtime clean

## Typical iteration loop

In one terminal:

    beethoven runtime run

In another:

    # edit sw/<testbench>.cc
    beethoven run            # auto-detects the daemon; won't tear it down
    # repeat
