# Command reference (CLI v1.0.0)

Verify against `beethoven <cmd> --help` of the installed binary; trust it over
this file. Global flags on every command: `-v`/`-vv` (verbose/trace — shows
full sbt/cmake output), `-q` (quiet), `-h`, `-V`.

## Project lifecycle

| Command | Purpose / key flags |
|---|---|
| `new <name>` | Scaffold `./<name>/`. `--platform <t>`, `--accel <Class>`, `--verilog`, `--vcs`. |
| `init` | Same, into the current dir (name = dir basename). |
| `check` | Parse manifest, check `sbt`/`cmake` (+ simulator) on PATH, elaborate Chisel. No codegen. |
| `info [--format text\|json]` | Resolved project/target/platform/prefix/libbeethoven. Works outside a project too. |
| `clean` | Remove `target/simulation/`. `--release` → `target/synthesis/`. `--all` → whole `target/` (incl. bindings). |

Platforms for `--platform`: `default`, `kria`, `kria2`, `aupzu3`, `aws-f1`,
`aws-f2`, `u200`, `u250`, `u280`, `baremetal`.

## Build and run

| Command | Purpose / key flags |
|---|---|
| `build [hw\|runtime\|sw]` | No slice = all three in order. `--release` = synthesis only; `--simulation` = simulation only (mutually exclusive); default builds both on synth-capable targets (only sim for `default`). `-j N`. Slices are strict: `runtime`/`sw` refuse if bindings or libbeethoven are missing. |
| `sim [TB] [-- args]` | Simulation end to end. `--no-build`, `--no-launch`, `--simulator icarus\|verilator\|vcs`. Not for `baremetal`. |
| `run [TB] [-- args]` | Real FPGA end to end (synthesis mode). `--no-build`, `--no-launch`. Not for `default`/`baremetal`. Needs root (`/dev/mem`). |
| `runtime build` | = `build runtime`. `--release`, `-j N`. |
| `runtime run` | Daemon in the foreground until Ctrl+C. `--release`, `--log-file <path>` (stderr only). Refuses if a daemon already holds this project's lock. |
| `runtime kill` | SIGKILL the daemon, delete lockfile + shmem segments + `target/<mode>/runtime/`. Idempotent. Rebuild runtime afterwards. |
| `runtime clean` | Delete `target/simulation/runtime/` (`--release`: synthesis). |
| `synth` | Vivado setup → synth → impl → bitstream (runs `build hw --release` first). `--up-to setup\|synth\|impl\|bit`, `--no-build`, `--gui`. Rejects `default`/`baremetal`. Zynq/Alveo — **not** the AWS F2 path. |
| `flash` | JTAG-program the latest bitstream via Vivado + `hw_server` at `localhost:3121`. Device pattern `xczu3*` (AUP-ZU3). |

`sim`/`run` lifecycle: probe daemon → (unless `--no-build`) build missing
pieces (daemon up: only `sw`; daemon down: `hw` if missing, `runtime`
incrementally, `sw` if missing) → launch daemon if down and wait ≤ 60 s for
ready → run testbench → stop daemon only if this invocation started it.
Testbench selection: positional name, else the only executable in
`target/sw/`, else error listing the candidates.

## AWS F2 helpers

| Command | Key flags |
|---|---|
| `aws upload` | `--host user@ip` (required), `--key <pem>`, `--remote-dir` (default `~/cl_beethoven_top`), `--delete`. Uploads `target/synthesis/aws/cl_beethoven_top/`. |
| `aws create-fpga-image` | Run on the compile VM. `--name`, `--region`, `--bucket`, `--cl-dir`, `--timing-report`, `--checkpoint-tar`, `--auto`, `--yes`, `--dry-run`. |
| `aws load` | Run on F2 (root). `--agfi` \| `--afi` \| `--name`, `--slot` (default 0), `--region`, `--yes`, `--dry-run`. Needs `fpga-load-local-image` (aws-fpga SDK). |

## Install and maintenance

| Command | Key flags |
|---|---|
| `setup` | `--prefix`, `--ref`, `--from`, `--hardware-from`, `--hardware-ref`, `--no-hardware`, `-j`. |
| `update` | Reinstall libbeethoven. `--ref`, `--from`, `-j`. |
| `uninstall` | `--yes` (always pass it). Removes install + cache + user config. |

## Exit codes

`0` success · `1` failure (build error, testbench failed, runtime error) ·
`2` usage error · `64` configuration error (not in a project, bad manifest,
missing prerequisite artifact, libbeethoven not installed) · `127` required
external tool missing from PATH.

## Paths

- User config: `~/.config/beethoven/config.toml` (prefix, checkout, ref).
- Install prefix: `~/.local` (libs under `lib/` or `lib64/`, cmake config
  under `lib*/cmake/beethoven/`, runtime sources under `share/beethoven/`).
- Beethoven-Hardware clone: `~/.local/share/beethoven/hardware-src`.
- Daemon lockfile: `${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}/beethoven-<uid>-<key>.lock`;
  contains the daemon PID once it is ready.
- Shared memory: `/dev/shm/compo_c_<uid>`, `/dev/shm/compo_d_<uid>`.

## Environment variables

`BEETHOVEN_PROJECT_ROOT` overrides project discovery (the CLI also sets it
on the daemon and testbench it launches; a stale exported value points every
command at the wrong project). `AWS_FPGA_REPO_DIR` is required for the
`aws-f2` release runtime build; `AWS_REGION` / `AWS_DEFAULT_REGION` feed the
`aws` helpers. `NO_COLOR` disables color. (`BEETHOVEN_PREFIX`, mentioned in
older docs, is not implemented — use `setup --prefix`.)
