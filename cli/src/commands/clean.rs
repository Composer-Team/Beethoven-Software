//! `beethoven clean` — remove build artifacts under `target/`.
//!
//! Defaults to wiping just `target/simulation/`. `--release` swaps to
//! `target/synthesis/`; `--all` removes the entire `target/` tree
//! (incl. `binding/` and `.sbt/` caches).

use crate::cli::CleanArgs;
use crate::error::Result;
use crate::state::Project;
use crate::ui;
use std::fs;

pub fn run(args: CleanArgs) -> Result<()> {
    let project = Project::discover()?;
    let target = project.root.join("target");

    let to_remove = if args.all {
        target
    } else if args.release {
        target.join("synthesis")
    } else {
        target.join("simulation")
    };

    if !to_remove.exists() {
        ui::print_note(&format!(
            "nothing to clean: {} does not exist",
            to_remove.display()
        ));
        return Ok(());
    }

    ui::print_stage("Removing", &to_remove.display().to_string());
    fs::remove_dir_all(&to_remove)?;
    ui::print_success("cleaned.");
    Ok(())
}
