//! `beethoven init` — scaffold a project in the current directory.

use crate::cli::InitArgs;
use crate::commands::scaffold::{self, ScaffoldCommand, ScaffoldRequest};
use crate::error::{CliError, Result};
use std::env;
use std::path::Path;

pub fn run(args: InitArgs) -> Result<()> {
    let dest = env::current_dir().map_err(|e| CliError::config(format!("cannot read cwd: {e}")))?;
    let name = project_name_from_dir(&dest)?;
    scaffold::validate_project_name(&name)?;

    if dest.join("Beethoven.toml").exists() {
        return Err(CliError::config(
            "Beethoven.toml already exists in this directory".to_string(),
        ));
    }

    scaffold::run(ScaffoldRequest {
        name,
        dest,
        platform: args.platform,
        accel: args.accel,
        verilog: args.verilog,
        vcs: args.vcs,
        command: ScaffoldCommand::Init,
    })
}

fn project_name_from_dir(dir: &Path) -> Result<String> {
    dir.file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| CliError::config(format!("cwd has no usable basename: {}", dir.display())))
        .map(str::to_string)
}
