//! `beethoven new <name>` — scaffold a project in a new directory.

use crate::cli::NewArgs;
use crate::commands::scaffold::{self, ScaffoldCommand, ScaffoldRequest};
use crate::error::{CliError, Result};
use std::env;

pub fn run(args: NewArgs) -> Result<()> {
    scaffold::validate_project_name(&args.name)?;

    let dest = env::current_dir()
        .map_err(|e| CliError::config(format!("cannot read cwd: {e}")))?
        .join(&args.name);

    if dest.exists() {
        return Err(CliError::config(format!(
            "destination already exists: {}",
            dest.display()
        )));
    }

    scaffold::run(ScaffoldRequest {
        name: args.name,
        dest,
        platform: args.platform,
        accel: args.accel,
        verilog: args.verilog,
        vcs: args.vcs,
        command: ScaffoldCommand::New,
    })
}
