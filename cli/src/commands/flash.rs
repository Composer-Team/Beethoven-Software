//! `beethoven flash` — JTAG-program the FPGA from the generated tcl.
//! Standalone Vivado batch — needs hw_server reachable (defaults to
//! localhost:3121 in `jtag_program.tcl`).

use crate::cli::FlashArgs;
use crate::core::exec;
use crate::error::{CliError, Result};
use crate::state::Project;
use crate::tools::vivado;
use crate::ui;

pub fn run(_args: FlashArgs) -> Result<()> {
    let project = Project::discover()?;
    exec::require_tool(
        "vivado",
        Some("install Xilinx Vivado and ensure `vivado` is on PATH"),
    )?;

    let impl_dir = project
        .root
        .join("target")
        .join("synthesis")
        .join("implementation");
    if !impl_dir.is_dir() {
        return Err(CliError::config(format!(
            "missing {}; run `beethoven synth` first to produce a bitstream",
            impl_dir.display()
        )));
    }
    // jtag_program.tcl is CLI-owned (no Scala emitter today) — write
    // it into impl_dir if it's not already there.
    vivado::ensure_cli_tcl(&impl_dir)?;

    ui::print_stage("Vivado", "JTAG program");
    vivado::run(&impl_dir, "batch", &["jtag_program.tcl"])?;
    ui::print_success("flash complete.");
    Ok(())
}
