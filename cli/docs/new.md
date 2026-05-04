# `beethoven new`

Scaffold a new Beethoven project in a new directory.

## Synopsis

    beethoven new <name> [--platform <p>] [--accel <name>] [--vcs]

## Description

Creates `./<name>/` and populates it with a minimal project skeleton:

    <name>/
    ├── Beethoven.toml           # project manifest (target = <platform>)
    ├── hw/
    │   ├── build.sbt            # depends on Beethoven-Hardware
    │   └── src/main/scala/<name>/
    │       └── <Accel>.scala    # accelerator stub extending AcceleratorConfig
    ├── sw/
    │   ├── CMakeLists.txt       # find_package(beethoven) + beethoven_build(...)
    │   └── <name>.cc            # testbench stub
    └── .gitignore

The `hw/` tree is a self-contained sbt project that depends on
`Beethoven-Hardware`. The `sw/` tree is built by the CLI against the
installed libbeethoven (see [`setup`](setup.md)).

## Options

| Flag | Default | Meaning |
|---|---|---|
| `--platform <p>` | from user config (`simulation`) | One of `simulation`, `kria`, `aupzu3`, `aws-f1`, `u200`, `baremetal`. |
| `--accel <name>` | PascalCased project name | Class name of the accelerator stub. |
| `--vcs` | (off) | Initialize a git repo and make an initial commit. |

## Inputs

- `~/.config/beethoven/config.toml` — for `default_platform` fallback.

## Outputs

- New directory `<name>/` with the skeleton above.
- Exit `0` on success.

## Errors

- `<name>/` already exists → exit `1`.
- Invalid platform → exit `2`.
- Cannot create directory (permission denied) → exit `1`.

## Examples

    beethoven new my-design
    beethoven new my-design --platform kria
    beethoven new my-design --accel VectorAdd --vcs

## Open questions

- **Template source.** Bundled in the CLI binary (simpler, ships
  in-sync with the CLI version) or pulled from a template repo
  (templates evolve independently)? Bundled wins for first cut.
- **Multi-accelerator scaffold.** `new` ships a single-accelerator
  skeleton; users add more by editing chisel. Sufficient for now.
