# Zynq flow (`kria`, `kria2`, `aupzu3`)

The pattern: **the host synthesizes and flashes; the board runs the user
program + runtime.**

- **Host**: x86 development machine with the full Beethoven setup, Vivado,
  and (for `beethoven flash`) the board's JTAG/USB cable attached.
- **Board**: the Zynq's ARM (aarch64) Linux. It shares DRAM with the FPGA
  fabric (programmable logic, "PL"). It needs only a deploy-only setup
  (`beethoven setup --no-hardware`, see setup.md).

Steps 1–3 run on the host, steps 4–7 on the board (over SSH).

## 1. Configure the target (host)

In `Beethoven.toml`:

```toml
[platform]
target = "aupzu3"          # or "kria" / "kria2"

[platform.aupzu3]
dram-size-gb = 8           # REQUIRED for aupzu3: 4 or 8
# memory-channels = 1
# clock-rate-mhz  = 100
```

Recommended before synthesis: simulate at this target's real bus widths with
`beethoven build --simulation && timeout 900 beethoven sim` (see simulate.md).

## 2. Synthesis + implementation + bitstream (host)

`vivado` must be on `PATH`, so source the Vivado settings script in the same
command (the path depends on the install, e.g. `/opt/Xilinx/2025.2/Vivado/settings64.sh`
or `/tools/Xilinx/Vivado/<ver>/settings64.sh`; ask the user if it is not obvious):

```bash
source /opt/Xilinx/2025.2/Vivado/settings64.sh && \
source "$HOME/.sdkman/bin/sdkman-init.sh" && \
beethoven synth
```

`synth` first runs `beethoven build hw --release` (Chisel → RTL + Vivado Tcl
under `target/synthesis/implementation/`), then runs Vivado in batch:
`0_setup.tcl` → `1_synth.tcl` → `2_impl.tcl` → `run_bitstream.tcl`. It
**always rebuilds from scratch** (`0_setup.tcl` wipes `xilinx_work/`) and takes
tens of minutes to hours: run it in a background shell with output redirected
to a log, or give the command to the user to run in `tmux`.

| Flag | Effect |
|---|---|
| `--up-to setup\|synth\|impl\|bit` | Stop early (default `bit`). `--up-to synth` is a quick resource/timing sanity check. |
| `--no-build` | Skip `build hw --release` (RTL already generated and unchanged). |
| `--gui` | Open the Vivado GUI on `0_setup.tcl` and exit — for the user to inspect interactively. |

Result: the bitstream at
`target/synthesis/implementation/xilinx_work/beethoven.runs/impl_1/*.bit`.
Check timing before deploying: look for negative WNS/TNS in
`impl_1/*timing_summary_routed.rpt`. A design that fails timing may still
produce a bitstream that misbehaves on the board.

## 3. Program the FPGA (host, JTAG)

```bash
source /opt/Xilinx/2025.2/Vivado/settings64.sh && beethoven flash
```

Requirements and limits:
- Run on the machine the JTAG cable is attached to; Xilinx `hw_server` must
  be reachable at `localhost:3121` (if the connect fails, start `hw_server`
  from Vivado's `bin/` first). Cable drivers must be installed once.
- **The generated Tcl selects devices matching `xczu3*` — i.e. AUP-ZU3.** For
  Kria (`xck26`) or other parts, `beethoven flash` will not find the device;
  program with the Vivado Hardware Manager, or load the `.bit` on the board
  (Linux FPGA manager, `fpgautil -b <bit>`, or `xmutil` — whatever the board
  image provides), e.g.
  `sudo cp design.bit /lib/firmware/ && echo design.bit | sudo tee /sys/class/fpga_manager/fpga0/firmware`.
- JTAG programming is volatile: re-flash after every board power cycle.
- Success ends with `✓ flash complete.`

## 4. Board one-time setup

On the board: install the **aarch64** CLI binary and run
`beethoven setup --no-hardware` (setup.md → Deploy-only machines). No sbt or
Vivado is needed there.

## 5. Copy the project to the board

The board must **not** receive x86 binaries, but **must** receive the
generated bindings and RTL (so `run` skips the hardware step, which would
need sbt):

```bash
rsync -az \
  --exclude=/target/sw/ --exclude=/target/synthesis/runtime/ \
  --exclude=/target/simulation/ --exclude=/target/synthesis/implementation/ \
  ./ <user>@<board>:~/my-design/
```

Why: `run` skips any build step whose output already *exists*. A copied x86
`target/sw/<tb>` would be "found" and executed on ARM (`Exec format error`).

## 6. Prepare the board (each boot)

Zynq allocations come from Linux huge pages, which must be reserved before
running (the allocator throws if there are not enough). Size them to the
program's buffers, e.g.:

```bash
echo 32 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
echo 4  | sudo tee /sys/kernel/mm/hugepages/hugepages-32768kB/nr_hugepages
```

Make sure the PL is programmed with the matching bitstream (step 3). The
bindings in `target/binding/` must come from the same build as the bitstream.

## 7. Build and run the user program + runtime (board)

Build as the normal user (so `target/` does not become root-owned), then run
under sudo — the runtime maps `/dev/mem` and UIO devices:

```bash
cd ~/my-design
beethoven build runtime --release && beethoven build sw
sudo -E env HOME="$HOME" PATH="$PATH" beethoven run --no-build [TESTBENCH] [-- <args>]
```

`HOME` keeps the CLI pointed at the user's `~/.config/beethoven`; `PATH`
lets sudo find `~/.local/bin/beethoven`. `run` launches `BeethovenRuntime`,
waits until it is ready, runs the testbench, then stops the daemon; the exit
code is the testbench's.

Split daemon/program (debugging): in one shell
`sudo -E env HOME="$HOME" PATH="$PATH" beethoven runtime run --release`,
in another `sudo -E env HOME="$HOME" PATH="$PATH" beethoven run --no-build`.
**Both must run as the same user (root)** — shared-memory names are per-uid.

Failures: see troubleshooting.md (`/dev/mem` permission, missing shmem file,
allocator/hugepage errors, hangs).
