//! `beethoven sim` — end-to-end simulation flow.
//!
//! Validates the project's target is something the daemon supports
//! (anything but `baremetal`), then delegates the build/launch/exec
//! lifecycle to `commands::pipeline::execute` with mode hard-coded to
//! `"simulation"`.

use crate::cli::SimArgs;
use crate::commands::pipeline;
use crate::error::{CliError, Result};
use crate::state::Project;

pub fn run(args: SimArgs) -> Result<()> {
    let project = Project::discover()?;

    match project.target() {
        Some("baremetal") => {
            return Err(CliError::config(
                "baremetal target has no daemon — `sim` / `run` don't apply".to_string(),
            ));
        }
        None => {
            return Err(CliError::config(
                "[platform].target is required to run sim".to_string(),
            ));
        }
        _ => {}
    }

    pipeline::execute(
        &project,
        "simulation",
        args.no_build,
        args.no_launch,
        args.testbench.as_deref(),
        args.simulator.as_deref(),
    )
}
