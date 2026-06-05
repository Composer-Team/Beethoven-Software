# `beethoven synth`

`beethoven synth` drives the Vivado synthesis/implementation/bitstream flow for
FPGA targets that emit a Vivado implementation directory.

Typical use from a Beethoven project root:

```bash
beethoven synth
```

Useful options:

```bash
beethoven synth --up-to setup   # generate/open the Vivado project setup only
beethoven synth --up-to synth   # stop after synth_design
beethoven synth --up-to impl    # stop after place/route implementation
beethoven synth --gui           # open Vivado GUI on 0_setup.tcl and exit
```

The command expects the hardware build to emit the project implementation Tcl
under:

```text
target/synthesis/implementation/
```

The CLI then runs the generated Tcl pipeline:

```text
0_setup.tcl
1_synth.tcl
2_impl.tcl
run_bitstream.tcl
```

`run_bitstream.tcl` is supplied by the CLI when the implementation directory
does not already contain one. It opens the generated Vivado project and launches
`impl_1` through `write_bitstream`.

## Flashing the generated bitstream

The matching host-side JTAG programmer is:

```bash
beethoven flash
```

See [`flash`](flash.md). `beethoven flash` is a Vivado/JTAG programmer for the
latest bitstream under `target/synthesis/implementation/`; it is not a generic
board-side Linux FPGA-manager loader and does not program nonvolatile boot flash.
