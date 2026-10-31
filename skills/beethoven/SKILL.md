---
name: beethoven
description: Drive Beethoven FPGA-accelerator projects through the `beethoven` CLI — install/setup on a new machine, scaffold projects, simulate designs, synthesize + implement for Zynq (kria/aupzu3) or AWS F2, flash Zynq bitstreams over JTAG, create/load AWS F2 FPGA images (AFI/AGFI), and run user software against the Beethoven runtime on real hardware. Use whenever the user mentions Beethoven, Beethoven.toml, BeethovenRuntime, libbeethoven, Beethoven-Hardware/Software/Zoo, or asks to set up, build, sim, synth, flash, or run a Beethoven design.
---

# Beethoven

Beethoven is a framework for building FPGA accelerators: hardware is written in
Chisel (or a Verilog blackbox), software is a C++ program that links
`libbeethoven` and talks to a per-project daemon, `BeethovenRuntime`, which owns
either a simulator (Icarus/Verilator/VCS) or the real FPGA.

**Every Beethoven operation goes through the `beethoven` CLI.** Do not call
sbt, cmake, vvp, or Vivado directly for things the CLI covers; it sets env
vars, paths, and build modes that hand-rolled commands get wrong.

## Ground rules (read before running anything)

1. **`beethoven <cmd> --help` is the source of truth.** If anything here
   disagrees with `--help` of the installed binary, trust `--help`. Check the
   version with `beethoven --version`.
2. **Never let a command wait on a prompt.** The agent shell has no TTY, so
   interactive prompts fail with `not a terminal`. Always pass:
   `uninstall --yes`, `aws create-fpga-image --name <n> --region <r> --yes --auto`,
   `aws load --agfi <agfi>` (or `--yes`). On macOS, install sbt + iverilog
   *before* `beethoven setup` (it otherwise opens a brew Y/n prompt).
3. **Wrap simulations in a timeout.** A wedged design runs forever and grows
   `target/simulation/dump.vcd` without bound. Use
   `timeout 600 beethoven sim ...` (scale to the design) and delete the VCD
   after a hang.
4. **Long or foreground commands go to the background or to the user.**
   `beethoven runtime run` never returns (foreground daemon). `beethoven synth`
   takes tens of minutes; the AWS F2 DCP build takes ~1.5 h — the user should
   run it inside `tmux`.
5. **Root and credentials belong to the user.** `sudo apt/dnf install`,
   `sudo beethoven run`, `sudo beethoven aws load`, hugepage writes, and
   `aws configure` need the user's password or secrets: show the exact command
   and ask them to run it (in Claude Code, `! <command>` runs it in-session).
   Never write AWS keys into files or commands yourself.
6. **Know the destructive commands.** `clean --all` deletes `target/`
   (including bindings). `runtime kill` SIGKILLs the daemon *and* deletes
   `target/<mode>/runtime/` — the next run must rebuild (no `--no-build`).
   Look before you delete.
7. **Zynq and AWS F2 are completely different flows.** Read
   `[platform].target` in `Beethoven.toml` first (or `beethoven info`) and
   follow the matching reference. Do not mix steps between them.

## Pick the flow

| User wants to… | Read |
|---|---|
| Set up Beethoven on a new machine / board / VM | [references/setup.md](references/setup.md) |
| Create a project, simulate a design, debug a sim | [references/simulate.md](references/simulate.md) |
| Zynq (`kria`, `kria2`, `aupzu3`): synth + impl, flash bitstream, run software on the board | [references/zynq.md](references/zynq.md) |
| AWS F2 (`aws-f2`): synth on a compile VM, create AFI, load AGFI, run software on F2 | [references/aws-f2.md](references/aws-f2.md) |
| Exact flags, exit codes, `target/` layout | [references/commands.md](references/commands.md) |
| Edit `Beethoven.toml` (targets, per-platform keys, simulator) | [references/manifest.md](references/manifest.md) |
| Something failed | [references/troubleshooting.md](references/troubleshooting.md) |

## The everyday loop (simulation)

```bash
beethoven new my-design            # or: beethoven new --verilog my-design
cd my-design
beethoven build                    # hw (sbt) + runtime (cmake) + sw (cmake)
timeout 600 beethoven sim          # launches daemon + simulator, runs testbench, tears down
```

**`sim`/`run` only build what is *missing*, not what is *stale*.** They skip
`hw` when generated RTL exists and skip `sw` when any testbench binary exists
in `target/sw/`. So after editing, rebuild explicitly before running:

- edited `sw/` → `beethoven build sw`
- edited `hw/` or `Beethoven.toml` → `beethoven build` (all three slices)

Forward testbench args after `--`: `beethoven sim my_tb -- --iterations 100`.
With several testbenches, name one positionally.

## Mental model

- Project root = the directory containing `Beethoven.toml` (the CLI walks up
  from cwd to find it). All output goes to `target/`.
- `build` has three slices: `hw` → `target/binding/` + `target/<mode>/hw/`;
  `runtime` → `target/<mode>/runtime/BeethovenRuntime`; `sw` → `target/sw/<tb>`.
- Mode is `simulation` or `synthesis`; it is chosen by the command, never by
  the manifest: `sim` / `build --simulation` → simulation; `run` / `synth` /
  `build --release` → synthesis. Plain `build` does both on synthesis-capable
  targets.
- `sim` and `run` share one lifecycle: probe for a running daemon → build what
  is missing → launch daemon if none → run testbench → stop the daemon only if
  they started it. The testbench's exit code is the command's exit code.
- One daemon per project at a time (enforced by a lockfile).

## Reporting back

After any flow, tell the user what ran, where artifacts are (bitstream path,
AGFI, testbench output), and the exact next command they need to run
themselves (sudo, tmux, credentials).
