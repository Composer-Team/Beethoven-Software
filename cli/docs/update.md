# `beethoven update`

Update libbeethoven to the latest commit on the tracked ref.

## Synopsis

    beethoven update [--ref <git-ref>] [--force]

## Description

Steps:

1. `git fetch` on the cached `Beethoven-Software` clone at
   `<checkout>` (recorded in `~/.config/beethoven/config.toml`).
2. Fast-forward to the tracked ref (or check out `--ref` if given,
   persisting the change to the user config).
3. Re-run `./install.sh --prefix <prefix>`.

Refuses to proceed if the cached clone has uncommitted local
modifications, unless `--force` is passed (which stashes them first).

## Options

| Flag | Default | Meaning |
|---|---|---|
| `--ref <ref>` | (current ref) | Switch to a different ref. |
| `--force` | (off) | Stash local changes before pulling. |

## Inputs

- `~/.config/beethoven/config.toml` (must exist; run [`setup`](setup.md) first).

## Outputs

- Reinstalled libbeethoven at `<prefix>`.
- Updated `~/.config/beethoven/config.toml` if `--ref` changed.

## Errors

- `setup` has not been run (no config.toml) → exit `64`.
- Local modifications in cached clone (and `--force` not passed) → exit `1`.
- Network failure during fetch → exit `1`.

## Examples

    beethoven update
    beethoven update --ref v0.2.0
    beethoven update --force
