# `beethoven check`

Validate the project's configuration and elaborate the chisel design
without producing build artifacts.

## Synopsis

    beethoven check

## Description

Performs three checks, in order. Each step short-circuits on failure.

1. **Manifest schema.** Parse `Beethoven.toml` and validate it against
   the schema in `docs/beethoven-toml-reference.md`. Reports unknown
   keys, missing required keys, and type mismatches with the offending
   line.
2. **Tool availability.** Verify that the tools needed by the declared
   target are on `PATH`: always `sbt` and `cmake`; for simulation,
   the chosen simulator binary (`verilator`, `iverilog`, `vcs`).
3. **Chisel elaboration.** Invoke sbt to elaborate the user's
   `AcceleratorConfig`. No verilog is emitted; cmake is not invoked;
   the runtime daemon is not built.

Intended for fast iteration — typically much faster than a full
`build` because it stops before codegen and runtime build.

## Inputs

- `<project>/Beethoven.toml`
- `<project>/hw/` chisel sources

## Outputs

- Diagnostics on stdout/stderr.
- Exit `0` on success.

## Errors

- Not in a project → exit `64`.
- Manifest validation failure (line + key reported) → exit `64`.
- Required tool not found (which tool, where to install) → exit `127`.
- Chisel compile failure → exit `1` (sbt's error passed through).

## Examples

    beethoven check
    beethoven check --verbose      # show full sbt output
