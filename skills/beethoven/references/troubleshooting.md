# Troubleshooting

Re-run the failing command with `-v` (or `-vv`) to see full sbt/cmake output.
Exit code hints: `127` tool missing, `64` configuration/prerequisite, `2`
usage, `1` build or runtime failure.

## Environment and setup

| Symptom | Cause → fix |
|---|---|
| `required tool 'sbt' not found on PATH` (exit 127) | SDKMAN not sourced in this shell: prefix `source ~/.sdkman/bin/sdkman-init.sh &&`. Same pattern for `vivado` (source Vivado `settings64.sh`), `cmake`, `iverilog`, `fpga-load-local-image` (aws-fpga SDK). |
| `could not read ... prompt: ... not a terminal` | An interactive prompt in a no-TTY shell. Add `--yes` (uninstall, aws), `--name/--region/--auto` (create-fpga-image), `--agfi` (load). On macOS, preinstall sbt + iverilog before `setup`. |
| `libbeethoven not installed` / `find_package(beethoven)` fails | Run `beethoven setup` (or `--no-hardware` on deploy-only machines); check `beethoven info`. |
| `not in a Beethoven project` (exit 64) | No `Beethoven.toml` in cwd or parents, or a stale exported `BEETHOVEN_PROJECT_ROOT`. |
| sbt dependency resolution fails on first build | Network/proxy issue, or `setup` was run with `--no-hardware` on a dev machine — rerun `beethoven setup`. |
| Build used old code after an edit | `sim`/`run` only build *missing* outputs. Run `beethoven build sw` (sw edits) or `beethoven build` (hw/manifest edits). |
| `multiple testbenches available (...)` | Pass the testbench name positionally. |

## Simulation

| Symptom | Cause → fix |
|---|---|
| sim never finishes | Design or memory path wedged. Always use `timeout`; then `rm -f target/simulation/*.vcd` (Icarus always traces, unbounded) and debug from the VCD (simulate.md). |
| After a timeout, the next `sim` says a daemon is up / runtime refuses to start | `timeout` killed the CLI but the daemon (`BeethovenRuntime`, or `vvp` for Icarus) survived. Its PID is in `${XDG_RUNTIME_DIR:-/tmp}/beethoven-$(id -u)-*.lock`; kill it. Or `beethoven runtime kill` (then rebuild the runtime — it deletes `target/<mode>/runtime/`). |
| Icarus: `I do not know how to elaborate assignment patterns using old method` | Generated SystemVerilog uses packed-array assignment patterns Icarus can't elaborate. Try `--simulator verilator`. (Known spot: an identity lookup table `_GEN` in `target/simulation/hw/TLToAXI4.sv`; a manual edit there is lost on the next `build hw`.) |
| Runtime fails to compile with Verilator (`VlWide<N>` / `data_channel.h`) | Known gap for `target = "default"` + Verilator (wide buses). Use Icarus for `default`, or Verilator with a real-FPGA target. |
| `find_package(verilator REQUIRED VERSION 5.0.0)` fails | Project sets `simulator = "verilator"` but Verilator ≥ 5 is not installed. |

## Real hardware (Zynq and F2)

| Symptom | Cause → fix |
|---|---|
| `Error opening /dev/mem. Errno: Permission denied`, `daemon exited before becoming ready: exit status: 13` | Not root. Use `sudo -E env HOME="$HOME" PATH="$PATH" ... beethoven run`. |
| `Error opening file /compo_c_<uid> No such file or directory` | No daemon running, or daemon and program run as different users (shared-memory names are per-uid). Run both under sudo, or let `run` launch the daemon. |
| `Exec format error` on a Zynq board | x86 binaries copied from the host. Delete `target/sw/` and `target/synthesis/runtime/` on the board, then `beethoven build runtime --release && beethoven build sw`. |
| Allocation throws on Zynq | Not enough huge pages reserved: `echo N \| sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages` (and the 32768kB class for big buffers). |
| Program hangs with no output on hardware | Bitstream and bindings from different builds, PL not programmed (or board power-cycled since flashing), or the AFI not loaded. Re-flash / re-load, then retry. CLI/runtime older than v1.0.0 also had a readiness race that hung at the first `malloc` — upgrade the CLI and run `beethoven update`. |
| Files in `target/` owned by root | A build ran under sudo. `sudo chown -R "$USER" target` and build as the user, run with `--no-build` under sudo. |
| `run --no-build` fails after `runtime kill` | `runtime kill` deleted the runtime build; run `beethoven build runtime --release` again. |
| `flash`: no device found | Device isn't `xczu3*` (only AUP-ZU3 supported by the generated Tcl), cable not attached, or `hw_server` not reachable at `localhost:3121`. |

## AWS F2

| Symptom | Cause → fix |
|---|---|
| `aws-f2 release runtime build requires the AWS FPGA SDK environment` | `cd ~/aws-fpga && source ./sdk_setup.sh && cd -` in the same command; pass `AWS_FPGA_REPO_DIR` through sudo. |
| `sdk_setup.sh`: `not a git repository`, `AWS_FPGA_REPO_DIR=unset` | Sourced from outside the repo; `cd ~/aws-fpga` first. |
| `fpga-load-local-image` not found | aws-fpga SDK not sourced on the F2 instance. |
| `ERROR: Please install/enable Vivado.` from `build_beethoven_f2.sh` | Non-login SSH shell; source `/opt/Xilinx/<ver>/Vivado/settings64.sh` first. |
| `create-fpga-image`: `Unable to locate credentials` | Compile VM has no `~/.aws/credentials`; the user copies them (aws-f2.md step 3). |
| `create-fpga-image` fails timing validation | The design failed post-route timing (check the timing report in the CL build output); fix the critical path in the hardware, then regenerate (`build hw --release`), re-upload, and rebuild. |
| `aws load` OK but `run` hangs | Wrong AGFI for this project's bindings, or the image is not `available` yet; check `describe-fpga-images`, reload with `sudo fpga-clear-local-image -S 0` then `aws load`. |
