# Configuration

The CLI consumes two layers of configuration: a per-user config that
names the install prefix and other defaults, and a per-project
`Beethoven.toml` that drives each build. Layers compose by precedence.

## User config — `~/.config/beethoven/config.toml`

Written by [`setup`](setup.md) and consulted by every subcommand that
needs to locate libbeethoven.

```toml
# install prefix used by setup/update/uninstall.
prefix = "/home/<you>/.local"

# absolute path to the cached Beethoven-Software clone (used by update).
checkout = "/home/<you>/.cache/beethoven/Beethoven-Software"

# git ref to track on `update` — "main" by default.
ref = "main"

# default platform for `new` / `init` when --platform is omitted.
default_platform = "default"
```

XDG-aware: `XDG_CONFIG_HOME` overrides `~/.config`; `XDG_CACHE_HOME`
overrides `~/.cache`.

## Project config — `Beethoven.toml`

Per-project schema is documented in
[`../../docs/beethoven-toml-reference.md`](../../docs/beethoven-toml-reference.md).
The CLI reads it; it does not mutate it.

## Environment variables

| Variable | Effect |
|---|---|
| `BEETHOVEN_PREFIX` | Override the install prefix (overrides `config.toml` `prefix`). |
| `BEETHOVEN_PROJECT_ROOT` | Override project root detection (default: walk up from cwd looking for `Beethoven.toml`). |
| `BEETHOVEN_PLATFORM` | Override the platform mapping for one invocation. |
| `XDG_CONFIG_HOME` | User config root. |
| `XDG_CACHE_HOME` | CLI cache root. |
| `NO_COLOR` | Disable color output. |

## libbeethoven discovery

The CLI finds libbeethoven via `find_package(beethoven)` driven by the
cmake **user package registry** (`~/.cmake/packages/beethoven/<md5>`).
`setup` writes that breadcrumb. The CLI never hardcodes paths — it
reads `BEETHOVEN_RUNTIME_SRC_DIR` exported by `beethovenConfig.cmake`.

## Precedence

When the same value is set at multiple levels, the higher entry wins:

1. CLI flag (e.g. `--platform foo`)
2. Environment variable (e.g. `BEETHOVEN_PLATFORM`)
3. Project `Beethoven.toml`
4. User `~/.config/beethoven/config.toml`
5. Built-in default

Not every subcommand reads every layer (`setup` doesn't need a project;
`tree` doesn't need a prefix). Per-subcommand docs list which layers
are consulted.
