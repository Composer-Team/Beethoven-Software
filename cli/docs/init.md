# `beethoven init`

Scaffold a Beethoven project in the **current** directory.

## Synopsis

    beethoven init [--platform <p>] [--accel <name>] [--vcs]

## Description

Behaves identically to [`new`](new.md), except it writes into the
current working directory instead of creating a new one. The project
name is derived from the basename of the current directory.

The directory does not need to be empty, but `init` refuses to proceed
if `Beethoven.toml` already exists.

If the directory is already a git repository, `--vcs` is a no-op.

## Options

Same as [`new`](new.md), minus the positional `<name>` argument.

## Errors

- `Beethoven.toml` already exists in the current directory → exit `1`.
- Invalid platform → exit `2`.
- Current directory's basename is not a valid project name → exit `2`.

## Examples

    mkdir my-design && cd my-design
    beethoven init
    beethoven init --platform aws-f1
    beethoven init --accel VectorAdd
