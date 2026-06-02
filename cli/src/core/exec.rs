//! Wrappers around `std::process::Command` for shelling out to
//! external tools (sbt, cmake, git). Centralizes the missing-tool
//! diagnostic so individual commands don't reinvent it.

use crate::error::{CliError, Result};
use std::path::{Path, PathBuf};
use std::process::{Command, ExitStatus};

/// Locate an external tool on PATH. Returns its absolute path or a
/// `MissingTool` error tagged with the supplied install hint (which
/// `ui::print_error` will surface as a `hint:` line).
pub fn require_tool(name: &str, hint: Option<&str>) -> Result<PathBuf> {
    which::which(name).map_err(|_| CliError::missing_tool(name.to_string(), hint.map(String::from)))
}

/// Run a command to completion; surface non-zero exit status as an
/// error. Stdout/stderr are inherited so the user sees tool output
/// in real time. Logs `[exec] <cmd>` before running so users can see
/// (and copy-paste-reproduce) every shell-out the CLI performs.
pub fn run(cmd: &mut Command) -> Result<ExitStatus> {
    crate::ui::print_exec(cmd);
    let program = cmd.get_program().to_owned();
    let status = cmd
        .status()
        .map_err(|e| anyhow::anyhow!("failed to execute {program:?}: {e}"))?;
    if !status.success() {
        return Err(anyhow::anyhow!("{program:?} exited with {status}").into());
    }
    Ok(status)
}

/// Build a `Command` with `cwd` set to `dir`. Convenience for the
/// common pattern of "run `tool` from inside the project root."
pub fn cmd_in(tool: impl AsRef<Path>, dir: impl AsRef<Path>) -> Command {
    let mut c = Command::new(tool.as_ref());
    c.current_dir(dir);
    c
}
