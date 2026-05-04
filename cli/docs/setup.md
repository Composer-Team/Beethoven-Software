# `beethoven setup`

First-run bootstrap: clone `Beethoven-Software`, build it, and install
libbeethoven into a user-writable prefix.

## Synopsis

    beethoven setup [--prefix <path>] [--ref <git-ref>] [--reinstall]

## Description

Performs the steps a first-time user would otherwise do by hand:

1. Clone `https://github.com/Composer-Team/Beethoven-Software` into
   `~/.cache/beethoven/Beethoven-Software` (or `git fetch` if a clone
   already exists).
2. Check out `--ref` (default: `main`).
3. Run `./install.sh --prefix <prefix>`.
4. Persist `prefix`, `checkout`, and `ref` to
   `~/.config/beethoven/config.toml`.

After `setup` succeeds, `find_package(beethoven)` works in any
downstream cmake project with no env vars or `CMAKE_PREFIX_PATH` —
`install.sh` writes the cmake user-package-registry breadcrumb at
`~/.cmake/packages/beethoven/<md5(prefix)>`.

`setup` is idempotent: running it again on an up-to-date checkout is a
fast no-op (or a re-install if `--reinstall` is passed).

## Options

| Flag | Default | Meaning |
|---|---|---|
| `--prefix <path>` | `~/.local` | Where to install libbeethoven. |
| `--ref <ref>` | `main` | Git ref to check out. |
| `--reinstall` | (off) | Force re-running `install.sh` even if the checkout has not changed. |

## Inputs

- Network access to clone the repo.
- Tools on `PATH`: `git`, `cmake`, `make`, a C++ compiler.

## Outputs

- Files under `<prefix>/lib`, `<prefix>/include`, `<prefix>/share/beethoven`,
  `<prefix>/lib/cmake/beethoven`.
- `~/.cmake/packages/beethoven/<md5>` registry breadcrumb.
- `~/.config/beethoven/config.toml` updated.

## Errors

- Network failure during clone → exit `1`.
- Compiler/toolchain missing → exit `127` with an OS-specific install hint.
- Permission denied writing prefix → exit `1` with a hint to use a
  user-writable prefix (e.g. `--prefix $HOME/.local`).

## Examples

    beethoven setup
    beethoven setup --prefix /opt/beethoven
    beethoven setup --ref refactor
    beethoven setup --reinstall

## Open questions

- Should the clone URL be configurable (for forks / mirrors)?
  Reasonable as `--repo <url>` later.
- Should `setup` support multiple parallel installs (per-ref prefixes)?
  Probably no — `~/.config/beethoven/config.toml` is single-tenant.
