//! `beethoven run` — end-to-end real-FPGA flow.
//!
//! Validates the project's target is real hardware (not `default`,
//! not `baremetal`), then delegates to `commands::pipeline::execute`
//! with mode hard-coded to `"synthesis"`.

use crate::cli::RunArgs;
use crate::commands::pipeline;
use crate::error::{CliError, Result};
use crate::state::Project;

pub fn run(args: RunArgs) -> Result<()> {
    let project = Project::discover()?;

    match project.target() {
        Some("default") => {
            return Err(CliError::config(
                "target = \"default\" can't run on real hardware; \
                 use `beethoven sim` instead"
                    .to_string(),
            ));
        }
        Some("baremetal") => {
            return Err(CliError::config(
                "baremetal target has no daemon — `sim` / `run` don't apply".to_string(),
            ));
        }
        None => {
            return Err(CliError::config(
                "[platform].target is required to run".to_string(),
            ));
        }
        _ => {}
    }

    pipeline::execute(
        &project,
        "synthesis",
        args.no_build,
        args.no_launch,
        args.testbench.as_deref(),
        None,
        &args.tb_args,
    )
}
