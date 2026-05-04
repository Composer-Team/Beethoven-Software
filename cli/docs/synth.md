# `beethoven synth`

**Placeholder.** Vivado-related commands (synthesis, place & route,
bitstream generation) are not yet implemented.

## Planned scope

When implemented, `synth` will manage the long-running steps that
turn the elaborated design into an FPGA bitstream:

- Synthesis (Vivado `synth_design`)
- Place & route (`opt_design`, `place_design`, `route_design`)
- Bitstream generation (`write_bitstream`)
- Reports (timing, utilization)

Likely a subgroup pattern, mirroring [`runtime`](runtime.md):

    beethoven synth {synth|impl|bitgen|all}

The CLI will not flash the resulting bitstream — that stays with
vendor tools (`xbutil`, `xsdb`) for now.

## Status

Not implemented. This doc reserves the namespace; the design will be
fleshed out separately.
