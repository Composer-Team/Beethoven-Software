# `beethoven info`

Print the CLI's resolved configuration.

## Synopsis

    beethoven info [--format <text|json>]

## Description

Prints what the CLI would use if you ran a build right now, after
applying every layer of configuration:

- Install prefix and where libbeethoven was found.
- `BEETHOVEN_RUNTIME_SRC_DIR` resolved from `beethovenConfig.cmake`.
- Project root (path containing `Beethoven.toml`).
- Project name and target.
- Resolved platform after target → platform mapping.
- Build mode (sim vs synth) for each entry in `[targets]`.
- Discovered `AcceleratorConfig` classes (best-effort — fast classpath
  scan, not full chisel elaboration).

Useful for debugging "why did it build for kria when I asked for sim".

When invoked **outside** a project, prints user-level info only
(prefix, runtime-src dir, default platform) and exits `0`.

## Options

| Flag | Default | Meaning |
|---|---|---|
| `--format text` | (default) | Human-readable. |
| `--format json` | | Single JSON object — for tooling and tests. |

## Outputs

Text format example:

    project        my-design
    project root   /home/me/code/my-design
    target         simulation
    platform       discrete
    install prefix /home/me/.local
    runtime-src    /home/me/.local/share/beethoven/runtime-src
    libbeethoven   /home/me/.local/lib64/libbeethoven-discrete.so

## Errors

- libbeethoven not installed → exit `64` with a hint to run
  [`setup`](setup.md).

## Examples

    beethoven info
    beethoven info --format json | jq .platform
