# Beethoven Software

This is the repo that contains both the simulator and FPGA management runtime. Documentation is hosted on the website.

## Dependencies

- cmake >= 4.0.0

You will need a simulator of some kind.
We currently support VCS, Icarus Verilog, and Verilator.
If you need support for something else, it should be trivial to support simulators that use VPI.
Others may require more intrusive modifications to the codebase.

** Linux **
```
apt-get install cmake verilator
```

** Mac **
```
brew install cmake verilator
```

