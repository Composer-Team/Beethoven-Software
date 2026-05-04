//! `vivado` wrapper. Drives the generated TCL pipeline at
//! `target/synthesis/implementation/`. The CLI shells out to Vivado in
//! either batch mode (for synth / impl / bit / flash) or GUI mode (so a
//! user can poke around interactively after a failed run).
//!
//! All callers run from the `implementation/` directory so Vivado's
//! `vivado.log` and `vivado.jou` land alongside the TCL files instead
//! of polluting the project root.

use crate::core::exec;
use crate::error::Result;
use std::path::Path;
use std::process::Command;

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
