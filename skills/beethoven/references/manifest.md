# `Beethoven.toml`

The project manifest, analogous to `Cargo.toml`. The CLI generates
`build.sbt` and the cmake builds from it; users edit only this file, `hw/`,
and `sw/`. Commented keys in the generated file show defaults.

```toml
[project]
name = "my_design"                    # REQUIRED

[hardware]
# src-dir = "hw"                      # default "hw"

[hardware.beethoven-hardware]         # framework dependency: exactly one of
version = "latest.integration"        #   newest published artifact (template default)
# version = "0.1.7-dev12"             #   pin a release (reproducible builds)
# path = "../Beethoven-Hardware"      #   local checkout (framework developers); wins if both set

# [software]
# src-dir = "sw"                      # default "sw"

[platform]
target = "default"                    # REQUIRED — see table
# simulator = "icarus"                # "icarus" (default) | "verilator" | "vcs"

[platform.default]                    # exactly one table matching `target`
# clock-rate-mhz = 100

# [build]
# output-dir = "target"               # default "target"
```

**Build mode is never in the manifest** — it comes from the command
(`sim`/`build --simulation` vs `run`/`synth`/`build --release`). A leftover
`[platform] build-mode` key is ignored. `simulator` lives directly under
`[platform]`, not in the per-target table.

`[software] default-testbench` appears in some docs but CLI v1.0.0 does not
read it: with several testbenches, pass the name positionally to `sim`/`run`.

## Targets and their tables

| `target` | Flow | `[platform.<target>]` keys |
|---|---|---|
| `default` | simulation only (no synthesis) | `clock-rate-mhz` (100) |
| `kria`, `kria2` | Zynq (zynq.md) | `memory-channels` (1), `clock-rate-mhz` (100) |
| `aupzu3` | Zynq (zynq.md) | **`dram-size-gb` = 4 or 8 (required, integer)**, `memory-channels` (1), `clock-rate-mhz` (100) |
| `aws-f2` | AWS F2 (aws-f2.md) | `remote-username` ("ubuntu") |
| `aws-f1` | legacy AWS F1 | **`memory-channels` 1–4 (required)**, `clock-recipe` "A0"/"A1"/"A2" |
| `u200`, `u250`, `u280` | Alveo cards | see Beethoven-Hardware `PlatformRegistry.scala` |
| `baremetal` | no daemon; `sim`/`run` refuse | — |

Switching `target` changes generated RTL and bindings: run `beethoven build`
afterwards (the old artifacts are not invalidated automatically; use
`beethoven clean --all` if in doubt).

## Common parse errors

- `Missing required key project.name`.
- `[hardware.beethoven-hardware] requires either path or version.`
- `Unknown [platform] target='X'.` — typo; see table.
- `Missing required key [platform.aupzu3] dram-size-gb.`
- `... must be an integer; got String` — wrote `"8"` instead of `8`.
