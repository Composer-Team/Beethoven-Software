# Open issues: simulation paths don't run end-to-end yet

Two distinct issues, both in the simulation path. Filing together since
they were uncovered while validating Phase 1–5 of the SW refactor.

---

## Issue 1: zynq + simulation + verilator end-to-end run hangs

## Status

Build chain works. Daemon launches. Shmem channels open. Testbench
connects. But `vector_add(...).get()` doesn't complete within several
minutes — daemon is at 99% CPU (Verilator simulating), testbench at 0%
CPU (blocked on shmem response).

Either Verilator is just very slow at this design, or there's a
deadlock between cmd_server / data_server / verilator's eval loop.
Cannot tell without instrumentation.

## Repro

```
template/Beethoven.toml:
  [platform]
  target     = "aupzu3"       # → BEETHOVEN_PLATFORM=zynq
  build-mode = "simulation"

# Inside template/:
sbt run

cmake -S /path/to/Beethoven-Software/runtime \
      -B target/simulation/runtime/_cmake \
      -DBEETHOVEN_PROJECT_ROOT=$PWD \
      -DBEETHOVEN_BUILD_MODE=simulation \
      -DBEETHOVEN_PLATFORM=zynq \
      -DBEETHOVEN_SIMULATOR=verilator
cmake --build target/simulation/runtime/_cmake -j

cmake -S sw -B target/sw \
      -DBEETHOVEN_PROJECT_ROOT=$PWD \
      -DBEETHOVEN_PLATFORM=zynq
cmake --build target/sw -j

DRAMCONFIG=/path/to/Beethoven-Software/runtime/DRAMsim3/configs/DDR4_8Gb_x16_3200.ini
./target/simulation/runtime/BeethovenRuntime -dramconfig $DRAMCONFIG &
./target/sw/vector_tb     # <— hangs here
```

## What works

- `cmake --build` clean (after the `!defined(SIM)` gating fixes in
  commit `7bb82e9`).
- Daemon writes `/dev/shm/compo_c_1000` and `/dev/shm/compo_d_1000`
  (cmd_server + data_server channels).
- Daemon log shows it past `Tracing!` (verilator FST trace started).
- Testbench process exists, attached to shmem, blocked but not crashed.
- `BeethovenRuntime` keeps simulating (CPU 99%) — not deadlocked
  daemon-internally; verilator's eval loop is making progress.

## What doesn't

- After ~3 minutes wall clock with daemon at 99% CPU, testbench is
  still at 0% CPU. No `vector_add` response delivered.
- `brt-kill` does not visibly tear down the daemon (probably blocked
  on the same IPC).
- Daemon's stdout shows no per-command tracing because the verilator
  frontend's `main()` doesn't parse `--verbose`/`-v` (only the FPGA
  path's `fpga_main.cc` does).

## Suspects

1. **Slow Verilator + insufficient patience.** Verilator's eval at this
   design size + unoptimized build (`-O0` is the default for
   `verilate(...)`) may genuinely take many minutes for 32-element
   vector_add. Fix: rebuild with `VERILATOR_ARGS -O3` and/or shrink
   `n_eles` in `vector_tb.cc` to bound the run.

2. **CMD_VALID never seen by RTL.** The frontend pokes `CMD_VALID=1`
   without a corresponding read of `CMD_READY`'s falling edge — but
   verilator's eval may step before the RTL latches the command.
   Worth verifying by enabling FST trace + dumping it.

3. **Response-poller / cmd-server lock-step issue.** In zynq mode,
   cmd_server takes `bus_lock` before `peek_mmio(CMD_READY)` (in the
   real-hardware path that we just gated out for SIM). The remaining
   sim path uses `cmds.push(addr.cmd)` instead. The verilator
   frontend's drain loop must consume from that queue. Worth checking
   whether the verilator frontend is actually pulling from the sim cmd
   queue or from MMIO.

4. **Memory subsystem mismatch.** The aupzu3 platform's RTL has DDR
   parameters that may not match the DRAMsim3 config we're feeding.
   `DDR4_8Gb_x16_3200.ini` is a generic config; aupzu3's actual DRAM
   may be DDR4-2400 or similar. A mismatched DDR could starve the
   accelerator's read requests.

## Next steps

In rough order of cost:

- [ ] **Add `-v`/`--verbose` parsing to verilator_axi_frontend.cc::main**
  so we can see [CMD_SERVER] / [DATA_SERVER] printf trail.
- [ ] **Shrink `vector_tb.cc` to `n_eles = 4`** and rerun. If it
  finishes in 10 sec, it's just slow Verilator.
- [ ] **Inspect the FST trace** (in `target/simulation/runtime/`).
  Look for `cmd_valid`, `cmd_ready`, M00_AXI activity. If CMD_VALID
  never asserts, frontend issue. If it asserts but the M00_AXI never
  goes valid, allocator/memory issue.
- [ ] **Try `target = "simulation"` (BEETHOVEN_PLATFORM=discrete)**
  to see whether the issue is zynq-specific (SimulationPlatform has
  sim-tuned timing constants — should be faster + simpler).
- [ ] **Check the AUPZU3 DRAM config** in Beethoven-Hardware and
  match the `dramconfig` to it.

## Workarounds considered (and rejected)

- Adding a hard deadline to `vector_tb.cc` (`exit(0)` after 30 sec)
  would mask any real issue. Want to actually verify e2e first.
- Bypassing libbeethoven's IPC and reading verilator's outputs
  directly defeats the architecture's whole point.

## What was fixed alongside this investigation

Three real bugs landed in `7bb82e9` while shaking out the zynq+sim
build:

- `mmio.cc:setup_mmio()` was opening `/dev/mem` in sim mode (would
  crash anywhere outside a Zynq board).
- `cmd_server.cc` had `bus_lock` lock/unlock outside the
  `(!defined(SIM))` gate, causing an unmatched unlock + linker error
  in sim builds.
- `data_server.cc` emitted ARM64 `DC CIVAC` cache-flush asm in sim
  builds, breaking compilation on x86_64 hosts.

These are all generic ZYNQ vs SIM gating issues — the runtime has
historically assumed `defined(ZYNQ)` ⇒ "real Zynq board." With
side-by-side platform variants and host-side simulation, that
assumption no longer holds.

---

## Issue 2: simulation target (BEETHOVEN_PLATFORM=discrete) does not compile

Switching the toml to `target = "simulation"` (which maps to
`BEETHOVEN_PLATFORM=discrete` and uses Beethoven-Hardware's
`SimulationPlatform`) re-exposes a template-specialization gap in the
runtime headers — it doesn't even build.

### Repro

```
template/Beethoven.toml:
  [platform]
  target     = "simulation"
  build-mode = "simulation"

# Inside template/:
sbt run

cmake -S /path/to/Beethoven-Software/runtime \
      -B target/simulation/runtime/_cmake \
      -DBEETHOVEN_PROJECT_ROOT=$PWD \
      -DBEETHOVEN_BUILD_MODE=simulation \
      -DBEETHOVEN_PLATFORM=discrete \
      -DBEETHOVEN_SIMULATOR=verilator
cmake --build target/simulation/runtime/_cmake -j
```

### Error

```
runtime/include/data_channel.h:91:32:
  error: invalid user-defined conversion from 'VlWide<4>' to 'uint32_t'

verilated_types.h:431:
  note: candidate is: VlWide<T_Words>::operator WDataOutP()
        [with T_Words = 4; WDataOutP = unsigned int*]'
```

### Cause

`data_channel<>` is a runtime template instantiated against verilator-
generated handle types. For the SimulationPlatform's data widths
(strb signal is 16 bytes = 128 bits = 4 × uint32_t), Verilator emits
the strb port as `VlWide<4>` (a 4-word "wide" type), not a plain
`uint32_t`.

`data_channel::getStrb(int i)` at line 91 does:

```cpp
uint32_t payload = strb.get(chunk32);
```

…assuming `strb.get(...)` returns something convertible to `uint32_t`.
For narrow signals it does (Verilator emits `uint32_t` directly). For
wide signals it returns `VlWide<4>`, which only converts to
`WDataOutP` (= `uint32_t*`), not `uint32_t`.

For the aupzu3/zynq path, this happens to compile because aupzu3's
bus widths land in the narrow range (strb fits in `uint32_t`).
SimulationPlatform's defaults (in `Beethoven-Hardware`) push them
wider.

### Fix sketch

`getStrb` should index into the wide payload directly:

```cpp
bool getStrb(int i) const {
    int chunk32 = i / 32;
    int subbit32 = i % 32;
    auto payload = strb.get();          // returns VlWide<N> for wide signals,
                                         // uint32_t / uint64_t for narrow
    if constexpr (...wide trait...) {
        return (payload[chunk32] >> subbit32) & 1;
    } else {
        return (payload >> i) & 1;
    }
}
```

…with a small SFINAE / `if constexpr` to handle both narrow and wide
verilator types. Probably also need to revisit other `data_channel`
methods (`getData`, `setStrb`, etc.) that make the same assumption.

### Status

This isn't a refactor regression — it's a pre-existing design coupling
that the old build flow happened to dodge by always running
SimulationPlatform with narrow widths set in scala. Worth a follow-up
that adds `if constexpr` overloads for `VlWide<N>` to data_channel /
address_channel / response_channel.

---

## Combined recommendation

For getting a green e2e demo *now*: run the discrete-platform path
against an explicitly narrow `target = "aws-f1"` config (or constrain
SimulationPlatform's bus widths in scala) until data_channel.h gains
`VlWide<N>` support. That sidesteps both issues.

For the framework long-term:

1. Add `if constexpr (verilator-wide)` paths to
   `data_channel.h` / `address_channel.h` / `response_channel.h`. This
   is what unblocks the SimulationPlatform path *and* future wide-bus
   designs on any platform.
2. Add `--verbose` parsing to the verilator frontend's main so we can
   actually trace the zynq+sim hang.
3. Decide whether the zynq+sim hang is "verilator is just slow"
   (acceptable, document the wait) or a real protocol mismatch
   (would surface from the trace dump).
