# CLI Code Structure

How `Beethoven-Software/cli/` is laid out, what each module owns, and
where to add new things. Companion to the per-subcommand specs in this
directory.

## Top-level layout

```
cli/
├── Cargo.toml                # crate manifest
├── docs/                     # spec docs (this dir)
├── template/                 # bundled project skeleton (embedded at compile time)
│   ├── .gitignore            #   verbatim
│   ├── .scalafmt.conf        #   verbatim
│   ├── Beethoven.toml.tmpl   #   {{name_snake}} {{target}} {{platform_specific}} ...
│   ├── build.sbt.tmpl        #   {{name_snake}}
│   ├── hw/                   #   chisel sources (filenames templated)
│   │   ├── {{accel}}.scala.tmpl
│   │   ├── {{accel}}Core.scala.tmpl
│   │   └── {{accel}}Config.scala.tmpl
│   └── sw/
│       ├── CMakeLists.txt.tmpl
│       └── {{name_snake}}_tb.cc.tmpl
└── src/
    ├── main.rs               # entry: parse argv, dispatch, exit code
    ├── lib.rs                # module declarations
    ├── cli.rs                # clap derive tree (the user-facing argv shape)
    ├── error.rs              # CliError + exit codes
    ├── ui.rs                 # colored print helpers + cargo-style stage prints
    ├── template.rs           # embedded-tree walker + {{var}} substitution
    │                         #   used by `new` / `init`
    │
    ├── commands/             # one file per subcommand
    │   ├── mod.rs            #   dispatcher (Command → handler) + not_yet_implemented
    │   ├── pipeline.rs       #   shared sim/run lifecycle (probe → build → launch →
    │   │                     #   exec testbench → teardown)
    │   └── *.rs              #   {new, init, clean, check, info, build,
    │                         #    runtime_cmd, sim, run, synth, setup,
    │                         #    update, uninstall}
    │
    ├── core/                 # foundations — paths, process exec, hashing
    │   ├── mod.rs
    │   ├── env.rs            #   XDG, project key (FNV-1a), lockfile path,
    │   │                     #   manifest path, expand_tilde, default_prefix
    │   └── exec.rs           #   require_tool (→ exit 127 if missing),
    │                         #   run (Command wrapper)
    │
    ├── state/                # parsed user / project state
    │   ├── mod.rs            #   re-exports UserConfig, Project, Manifest,
    │   │                     #   Platform, target_to_platform
    │   ├── config.rs         #   ~/.config/beethoven/config.toml
    │   └── project.rs        #   Beethoven.toml manifest + target→platform
    │                         #   mapping
    │
    ├── tools/                # external program wrappers
    │   ├── mod.rs
    │   ├── git.rs            #   clone, checkout (shells out to `git`)
    │   ├── sbt.rs            #   `sbt run` wrapper (used by `build hw`)
    │   └── cmake.rs          #   configure (generic -D defines), build, install,
    │                         #   copy_manifest, uninstall_from_manifest,
    │                         #   registry sweep, find_installed_libs,
    │                         #   parse_runtime_src_dir
    │
    └── runtime/              # daemon-side concerns
        ├── mod.rs
        ├── lifecycle.rs      #   build_launch_command (verilator / icarus / vcs / synth
        │                     #   dispatch), runtime_out_dir, report_exit
        └── probe.rs          #   flock-based "is BeethovenRuntime up?" probe
```

Top-level `src/` is six files: the four user-facing ones (`main`,
`lib`, `cli`, `error`, `ui`) plus `template.rs` for the scaffolding
feature. Everything else lives in one of the five subdirectories.

The `template/` directory next to `src/` is the embedded scaffolding
content — included into the binary at compile time via
`include_dir!("$CARGO_MANIFEST_DIR/template")`. Files ending in
`.tmpl` get placeholder substitution at extraction time; everything
else copies verbatim.

## Module responsibilities

| Module | Owns |
|---|---|
| `cli` | The shape of `beethoven <command> [args]` as parsed by clap. |
| `error` | `CliError` enum, exit-code mapping, conversion from `io::Error` / `anyhow::Error`. The shell contract documented in `cli/docs/README.md`. |
| `ui` | Every coloured terminal output goes through here: `print_error`, `print_warning`, `print_note`, `print_success`, `print_stage`. Honors `NO_COLOR` and TTY detection via `console`. |
| `template` | Embedded `cli/template/` tree + the `{{name}} / {{name_snake}} / {{accel}} / {{system}} / {{target}} / {{platform_specific}}` substitution. Used by `commands::new` and `commands::init`. |
| `commands` | One handler per subcommand. The handler signature is `fn run(args: Args) -> Result<()>`; the dispatcher in `commands/mod.rs` is the only place that knows about every command's existence. |
| `core` | "Things every other module reaches for" — XDG path resolution, project-key hashing, FS lockfile path derivation, `Command` wrappers. No domain knowledge. |
| `state` | Parsed `Beethoven.toml` (per project) and `config.toml` (per user). The `Platform` enum and `target_to_platform()` mapping live here too — they're a property of how we interpret the manifest. |
| `tools` | Wrappers around external programs. Each submodule (`git`, `cmake`) owns the argv construction and error mapping for its tool; nothing else in the crate should `Command::new("git")` or `cmake` directly. |
| `runtime` | Daemon-side concerns. Currently just the flock probe; phase 6 adds spawn / wait-ready / shutdown lifecycle helpers. |

## Dependency direction

Layered, with no cycles:

```
commands  ──→  state, tools, runtime, core, ui, error, cli
state     ──→  core, error
tools     ──→  core, error
runtime   ──→  core, error
ui        ──→  error
core      ──→  error
error     ──→  (none — just std + thiserror + anyhow)
```

Read this as: an arrow means "may import from." `commands` is at the
top of the food chain; `error` is at the bottom. `cli` (the clap tree)
is referenced by `main` and `commands`, but `cli` itself doesn't
import from anywhere else in the crate.

## How to add things

### A new subcommand

1. Add a variant to `Command` in `src/cli.rs`, plus an `Args` struct
   if the command takes options.
2. Create `src/commands/<name>.rs` with `pub fn run(args: <Args>) -> Result<()>`.
3. Add `pub mod <name>;` to `src/commands/mod.rs` and the matching arm
   in `dispatch`.
4. Write a spec at `cli/docs/<name>.md` and link it from
   `cli/docs/README.md`'s command table.

### A new external tool wrapper

1. Add a submodule under `src/tools/` (e.g. `src/tools/sbt.rs`).
2. Use `core::exec::require_tool` for the not-on-PATH error and
   `core::exec::run` for the actual invocation.
3. Add `pub mod sbt;` to `src/tools/mod.rs`.

### A new piece of project state

1. Extend `state/project.rs` (for things derived from `Beethoven.toml`)
   or `state/config.rs` (for things in `~/.config/beethoven/config.toml`).
2. If the type is widely used, add a re-export in `src/state/mod.rs`.

### A new printable element

1. Add a helper in `src/ui.rs` (e.g. `print_progress(msg: &str)`).
2. Use `console::style` for the styling so TTY detection /
   `NO_COLOR` behave correctly.

## Conventions

- **Errors:** return `CliError` from every public handler. Use
  `anyhow::anyhow!(...).into()` for one-off errors, `CliError::config`
  / `CliError::usage` / `CliError::missing_tool` for the typed cases.
- **Subprocess output:** inherit stdio by default so the user sees
  cmake/git in real time. `core::exec::run` does this.
- **Tempdirs:** `tempfile::Builder::new().prefix("beethoven-…")` so
  abandoned tempdirs are easy to spot. They auto-clean on Drop, even
  on early bail.
- **Comments:** `//!` module-level summary at the top of each file
  explaining purpose and any non-obvious design decisions. Inline
  comments only where the *why* isn't apparent from the code.
- **No `mod.rs`-only files for trivia:** if a directory has just one
  submodule, the parent file (`runtime.rs`) is fine; we use `mod.rs`
  in the directory once there are multiple submodules and the parent
  becomes a re-export hub.

## Verification

After any structural change:

```bash
cargo build
./target/debug/beethoven --help
./target/debug/beethoven info
./target/debug/beethoven info --format json
./target/debug/beethoven check    # in a fixture project
```

Output should be styled (color in TTY, plain when piped),
greppable, and produce stable exit codes per `cli/docs/README.md`.
