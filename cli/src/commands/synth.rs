//! `beethoven synth` — drive Vivado from the generated TCL pipeline.
//!
//! Two-batch flow:
//!   1. `vivado -mode batch -source 0_setup.tcl -source 1_synth.tcl -source 2_impl.tcl`
//!      (one Vivado session — `1_synth.tcl` and `2_impl.tcl` reference
//!      `get_runs synth_1`/`impl_1` from the project `0_setup.tcl` opens.)
//!   2. `vivado -mode batch -source run_bitstream.tcl`
//!      (standalone — opens its own xpr.)
//!
//! Always full rebuild — `0_setup.tcl` does `rm -rf xilinx_work`. Use
//! `beethoven flash` to JTAG-program the resulting bitstream.

use crate::cli::{SynthArgs, SynthStage};
use crate::commands::build;
use crate::core::exec;
use crate::error::{CliError, Result};
use crate::state::Project;
use crate::tools::vivado;
use crate::ui;
use std::path::{Path, PathBuf};

pub fn run(args: SynthArgs) -> Result<()> {
    let project = Project::discover()?;

    match project.target() {
        Some("default") => {
            return Err(CliError::config(
                "target = \"default\" can't be synthesized; \
                 set [platform].target to a real-FPGA target".to_string(),
            ));
        }
        Some("baremetal") => {
            return Err(CliError::config(
                "target = \"baremetal\" has no FPGA bitstream to produce".to_string(),
            ));
        }
        None => {
            return Err(CliError::config(
                "[platform].target is required to synth".to_string(),
            ));
        }
        _ => {}
    }

    exec::require_tool(
        "vivado",
        Some("install Xilinx Vivado and ensure `vivado` is on PATH"),
    )?;

    if !args.no_build {
        build::build_hw(&project, "synthesis")?;
    }

    let impl_dir = project
        .root
        .join("target")
        .join("synthesis")
        .join("implementation");
    require_dir(&impl_dir)?;
    // 0/1/2_*.tcl come from Beethoven-Hardware's SynthScript via
    // `build hw --release`. run_bitstream.tcl is CLI-owned (no Scala
    // emitter today) — write it here if it's missing.
    require_files(&impl_dir, &["0_setup.tcl", "1_synth.tcl", "2_impl.tcl"])?;
    vivado::ensure_cli_tcl(&impl_dir)?;

    if args.gui {
        // Hand off to Vivado GUI on the setup script; user drives the rest.
        ui::print_stage("Launching", "Vivado GUI on 0_setup.tcl");
        return vivado::run(&impl_dir, "gui", &["0_setup.tcl"]);
    }

    let stop_after = args.up_to.unwrap_or(SynthStage::Bit);

    // Batch 1: setup [+ synth] [+ impl]. Single Vivado session.
    let mut batch1: Vec<&str> = vec!["0_setup.tcl"];
    let mut label = String::from("setup");
    if matches!(stop_after, SynthStage::Synth | SynthStage::Impl | SynthStage::Bit) {
        batch1.push("1_synth.tcl");
        label.push_str(" + synth");
    }
    if matches!(stop_after, SynthStage::Impl | SynthStage::Bit) {
        batch1.push("2_impl.tcl");
        label.push_str(" + impl");
    }
    ui::print_stage("Vivado", &label);
    vivado::run(&impl_dir, "batch", &batch1)?;

    if matches!(stop_after, SynthStage::Bit) {
        ui::print_stage("Vivado", "bitstream");
        vivado::run(&impl_dir, "batch", &["run_bitstream.tcl"])?;
        match find_bitstream(&impl_dir) {
            Some(bit) => ui::print_success(&format!("bitstream: {}", bit.display())),
            None => ui::print_warning(
                "bitstream stage finished but no .bit found under \
                 xilinx_work/beethoven.runs/impl_1/",
            ),
        }
    } else {
        ui::print_success(&format!("synth complete (stopped after {:?})", stop_after));
    }

    Ok(())
}

fn require_dir(p: &Path) -> Result<()> {
    if p.is_dir() {
        Ok(())
    } else {
        Err(CliError::config(format!(
            "missing {}; run `beethoven build hw --release` first",
            p.display()
        )))
    }
}

fn require_files(dir: &Path, names: &[&str]) -> Result<()> {
    for n in names {
        let p = dir.join(n);
        if !p.is_file() {
            return Err(CliError::config(format!(
                "missing {}; regenerate via `beethoven build hw --release`",
                p.display()
            )));
        }
    }
    Ok(())
}

fn find_bitstream(impl_dir: &Path) -> Option<PathBuf> {
    let runs = impl_dir.join("xilinx_work/beethoven.runs/impl_1");
    std::fs::read_dir(&runs)
        .ok()?
        .flatten()
        .map(|e| e.path())
        .find(|p| p.extension().and_then(|x| x.to_str()) == Some("bit"))
}
