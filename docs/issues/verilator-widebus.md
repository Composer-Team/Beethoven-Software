# Open issue: data_channel.h doesn't support Verilator's wide-signal types

`target = "simulation"` + `build-mode = "simulation"` + `BEETHOVEN_SIMULATOR=verilator`
fails to compile the runtime daemon. The SimulationPlatform's wider bus
widths cause Verilator to emit `VlWide<N>` types for some signals, and
`runtime/include/core/data_channel.h` doesn't know how to decode them.

## Repro

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

## Error

```
runtime/include/core/data_channel.h:91:32:
  error: invalid user-defined conversion from 'VlWide<4>' to 'uint32_t'

verilated_types.h:431:
  note: candidate is: VlWide<T_Words>::operator WDataOutP()
        [with T_Words = 4; WDataOutP = unsigned int*]'
```

## Cause

`data_channel<>` is a runtime template instantiated against
verilator-generated handle types. For SimulationPlatform's data widths
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

For aupzu3/zynq targets this happens to compile because their
bus widths land in the narrow range (strb fits in `uint32_t`).
SimulationPlatform's defaults push them wider.

## Fix sketch

`getStrb` should index into the wide payload directly:

```cpp
bool getStrb(int i) const {
    int chunk32 = i / 32;
    int subbit32 = i % 32;
    auto payload = strb.get();          // VlWide<N> for wide signals,
                                         // uint32_t/uint64_t for narrow
    if constexpr (/*is VlWide*/) {
        return (payload[chunk32] >> subbit32) & 1;
    } else {
        return (payload >> i) & 1;
    }
}
```

…with a small SFINAE / `if constexpr` to handle both narrow and wide
verilator types. Likely also need overloads in other `data_channel`
methods (`getData`, `setStrb`, etc.) that make the same assumption.

The same gap probably affects `address_channel.h` and
`response_channel.h` — worth auditing all three when fixing.

## Workaround

Run with `target = "aws-f1"` (or any narrow-bus discrete target) when
verifying with Verilator. Or use Icarus, which doesn't go through these
templates the same way — `target = "simulation"` with Icarus already
works end-to-end.

## Status

Not a refactor regression — it's a pre-existing design coupling that
the old build flow happened to dodge by always pairing
SimulationPlatform with narrow widths in scala. Surfaces now because
Phase 3's runtime cmake tries to build for SimulationPlatform's
defaults verbatim.
