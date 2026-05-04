# `beethoven uninstall`

Remove libbeethoven from the install prefix.

## Synopsis

    beethoven uninstall [--yes] [--purge]

## Description

Runs `uninstall.sh` against the prefix recorded in
`~/.config/beethoven/config.toml`. The script removes installed libs,
headers, cmake configs, the runtime source-package, and the matching
cmake user-package-registry entry — but only entries whose target path
points inside the prefix.

`--purge` additionally removes the cached clone
(`~/.cache/beethoven/`) and the user config
(`~/.config/beethoven/`). After `--purge`, the next CLI run is
indistinguishable from a fresh install.

By default, prompts for confirmation before deleting. `--yes` skips
the prompt.

## Options

| Flag | Meaning |
|---|---|
| `--yes` | Skip the confirmation prompt. |
| `--purge` | Also remove the cache and the user config. |

## Inputs

- `~/.config/beethoven/config.toml` (for `prefix` and, with
  `--purge`, `checkout`).

## Outputs

- Files removed under `<prefix>` (libs, headers, cmake configs, share dir).
- With `--purge`: `~/.cache/beethoven` and `~/.config/beethoven` removed.

## Errors

- `setup` has not been run (nothing to uninstall) → exit `0` with a
  message (idempotent — it is fine to uninstall something that isn't
  there).
- Permission denied removing files → exit `1`.

## Examples

    beethoven uninstall
    beethoven uninstall --yes
    beethoven uninstall --yes --purge
