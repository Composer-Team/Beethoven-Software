# Beethoven CLI

`beethoven` is the command-line tool that orchestrates Beethoven projects:
hardware generation (sbt), runtime build (cmake), user testbench build
(cmake), and execution. It is a thin orchestrator — every step shells out
to the underlying tool, and each subcommand has a focused responsibility.

## Synopsis

    beethoven [OPTIONS] <COMMAND>

## Global options

| Flag | Meaning |
|---|---|
| `-V`, `--version` | Print CLI version and exit. |
| `-h`, `--help` | Print top-level help. |
| `-v`, `--verbose` | Verbose output (`-vv` for trace). |
| `-q`, `--quiet` | Suppress non-error output. |
| `--color <auto\|always\|never>` | Color control. |

## Commands

### Project lifecycle

| Command | One-liner |
|---|---|
| [`new`](new.md) | Scaffold a new project in a new directory. |
| [`init`](init.md) | Scaffold in the current directory. |
| [`clean`](clean.md) | Remove build artifacts under `target/`. |
| [`check`](check.md) | Validate `Beethoven.toml` and elaborate the chisel design (no codegen). |
| [`info`](info.md) | Print the resolved configuration. |

### Build & run

| Command | One-liner |
|---|---|
| [`build`](build.md) | Build hw, runtime, and user sw — sliced as `build {hw\|runtime\|sw}`. |
| [`sim`](sim.md) | Build and run the project end-to-end in simulation. Auto-detects an existing daemon. |
| [`run`](run.md) | Build and run the project on real FPGA. Auto-detects an existing daemon. |
| [`runtime`](runtime.md) | Manage the runtime daemon: `runtime {build\|run\|clean}`. |
| [`synth`](synth.md) | (Placeholder) Vivado synth / P&R / bitgen. |

### Install & maintenance

| Command | One-liner |
|---|---|
| [`setup`](setup.md) | First-run bootstrap: clone Beethoven-Software and install libbeethoven. |
| [`update`](update.md) | Pull the cached clone and reinstall libbeethoven. |
| [`uninstall`](uninstall.md) | Remove libbeethoven from the install prefix. |

`synth` is a reserved namespace — not yet implemented.

## Lifecycle

Typical first-run journey:

    beethoven setup           # one-time, machine-wide install of libbeethoven
    beethoven new my-design   # creates ./my-design/ with Beethoven.toml, hw/, sw/
    cd my-design
    beethoven check           # validate manifest and elaborate chisel
    beethoven build           # generate verilog, runtime daemon, testbench (sim + synth when supported)
    beethoven sim             # for target=default: end-to-end run
    # OR
    beethoven run             # for real FPGA: end-to-end run

For a tight iteration loop, run the daemon once and let `run` /
`sim` auto-detect it between edits:

    # terminal A
    beethoven runtime run
    # terminal B
    beethoven run             # auto-detects daemon; won't tear it down

## Exit codes

| Code | Meaning |
|---|---|
| `0` | Success. |
| `1` | Generic failure (build error, validation error, IO error). |
| `2` | Usage error (bad flags, missing args). |
| `64` | Configuration error (malformed `Beethoven.toml`, missing prefix). |
| `127` | A required external tool is missing (sbt, cmake, git, …). |

## Where things live

- **User config:** `~/.config/beethoven/config.toml` (XDG-aware).
- **CLI cache:** `~/.cache/beethoven/` (cloned `Beethoven-Software`, build temp).
- **Install prefix:** `~/.local` by default, set by `setup --prefix`.
- **Per-project artifacts:** `<project>/target/<mode>/`.

See [`config.md`](config.md) for full details.
