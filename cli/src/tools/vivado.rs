//! `vivado` wrapper. Drives the generated TCL pipeline at
//! `target/synthesis/implementation/`. The CLI shells out to Vivado in
//! either batch mode (for synth / impl / bit / flash) or GUI mode (so a
//! user can poke around interactively after a failed run).
//!
//! All callers run from the `implementation/` directory so Vivado's
//! `vivado.log` and `vivado.jou` land alongside the TCL files instead
//! of polluting the project root.

use crate::core::exec;
use crate::error::{CliError, Result};
use std::fs;
use std::path::Path;
use std::process::Command;

/// Bitstream-stage TCL: opens the synthesized project and runs the
/// impl_1 run through `write_bitstream`. Generic — no project-specific
/// values — so we keep it baked into the CLI rather than templating it
/// per-project. Beethoven-Hardware's Scala `SynthScript.write_to_dir`
/// only emits 0/1/2_*.tcl; the bit step is the CLI's responsibility.
const RUN_BITSTREAM_TCL: &str = r#"open_project xilinx_work/beethoven.xpr
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
puts "Bitstream artifacts:"
foreach f [glob -nocomplain xilinx_work/beethoven.runs/impl_1/*.bit xilinx_work/beethoven.runs/impl_1/*.bin] {
    puts "  $f"
}
"#;

/// JTAG-flash TCL. The device-id pattern is hardcoded to `xczu3*`
/// (AUP-ZU3 / Zynq UltraScale+ MPSoC) for now — that's the only
/// platform the CLI's `flash` subcommand has been exercised against.
/// When we add a second JTAG-flashable target, lift this into a per-
/// platform template.
const JTAG_PROGRAM_TCL: &str = r#"open_hw_manager
connect_hw_server -url localhost:3121 -allow_non_jtag
open_hw_target [lindex [get_hw_targets] 0]
set dev [lindex [get_hw_devices xczu3*] 0]
current_hw_device $dev
refresh_hw_device -update_hw_probes false $dev
set_property PROGRAM.FILE [glob xilinx_work/beethoven.runs/impl_1/*.bit] $dev
program_hw_devices $dev
refresh_hw_device $dev
puts "=== post-program done state ==="
puts "  device : $dev"
close_hw_target
disconnect_hw_server
"#;

/// Idempotently write the CLI-owned TCL files into `impl_dir`. Called
/// by `synth`/`flash` before the corresponding Vivado batch runs.
/// Existing files are left alone — if Beethoven-Hardware ever grows a
/// per-platform emitter for these, its output wins.
pub fn ensure_cli_tcl(impl_dir: &Path) -> Result<()> {
    write_if_missing(impl_dir.join("run_bitstream.tcl"), RUN_BITSTREAM_TCL)?;
    write_if_missing(impl_dir.join("jtag_program.tcl"), JTAG_PROGRAM_TCL)?;
    Ok(())
}

fn write_if_missing(path: std::path::PathBuf, contents: &str) -> Result<()> {
    if path.exists() {
        return Ok(());
    }
    fs::write(&path, contents).map_err(|e| {
        CliError::config(format!("cannot write {}: {e}", path.display()))
    })?;
    Ok(())
}

/// Run `vivado -mode <mode> -source f1 -source f2 ...` from `cwd`.
/// Inherits stdio so the user sees Vivado's progress in real time.
///
/// `mode` is one of `"batch"` or `"gui"`. Multiple `-source` files are
/// honored as a single Vivado session — important for `0_setup.tcl +
/// 1_synth.tcl + 2_impl.tcl`, which share project state via
/// `get_runs synth_1` / `impl_1`.
pub fn run(cwd: &Path, mode: &str, sources: &[&str]) -> Result<()> {
    let mut cmd = Command::new("vivado");
    cmd.current_dir(cwd).arg("-mode").arg(mode);
    for s in sources {
        cmd.arg("-source").arg(s);
    }
    exec::run(&mut cmd)?;
    Ok(())
}
