# `beethoven clean`

Remove build artifacts from the project's `target/` directory.

## Synopsis

    beethoven clean [--release] [--all]

## Description

By default, removes `target/sim/` only. `--release` removes
`target/synth/` instead. `--all` removes the entire `target/` directory
(every build mode plus `target/binding/`, the chisel-generated bindings
shared between modes).

The `target/` directory is recreated by the next build.

`clean` does **not** touch the installed libbeethoven, the cache, or
the user config — for that, see [`uninstall`](uninstall.md).

## Options

| Flag | Meaning |
|---|---|
| `--release` | Remove `target/synth/` instead of `target/sim/`. |
| `--all` | Remove the entire `target/` directory. |

## Inputs

- Project root (walked up from cwd looking for `Beethoven.toml`).

## Outputs

- Files removed under `<project>/target/`.

## Errors

- Not in a project (no `Beethoven.toml` found) → exit `64`.
- `target/` does not exist → exit `0` with a message (idempotent).

## Examples

    beethoven clean              # wipe target/sim/
    beethoven clean --release    # wipe target/synth/
    beethoven clean --all        # wipe everything under target/
