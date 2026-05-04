//! `sbt` wrapper. The project's `build.sbt` sets `mainClass` to
//! `beethoven.cli.Run`, which reads `Beethoven.toml` and emits chisel
//! verilog + bindings under `target/binding/` and `target/<mode>/hw/`.
//! So "build hw" is just `sbt run` from the project root.

use crate::core::exec;
use crate::error::Result;
use std::path::Path;
use std::process::Command;

/// `sbt "run --mode <mode>"` from `project_root`. Inherits stdio so
/// the user sees sbt's dependency resolution / compile / run output
/// in real time.
///
/// `mode` is either `"simulation"` or `"synthesis"` and is required —
/// `beethoven.cli.Run` (the build.sbt mainClass) refuses to run
/// without `--mode`. Build mode is per-invocation, not in the
/// manifest; the caller (typically `commands::build::build_hw`)
/// derives it from the CLI command (`sim`→simulation, `run`→
/// synthesis, `build [--release]`).
///
/// Why a single argv string: sbt parses each argv as a separate
/// top-level command, so passing `["run", "--mode", "simulation"]`
/// would make sbt try to run three separate tasks. Quoting them
/// together as one arg makes sbt pass them as a unit to the `run`
/// task's argv.
pub fn run(project_root: &Path, mode: &str) -> Result<()> {
    let mut cmd = Command::new("sbt");
    cmd.arg(format!("run --mode {mode}"))
        .current_dir(project_root);
    exec::run(&mut cmd)?;
    Ok(())
}

/// `sbt publishLocal` from `project_root`. Writes the project's jar +
/// pom into `~/.ivy2/local/<org>/<name>_<scala-ver>/<version>/` so
/// any sibling sbt project resolves it without hitting the network.
/// `setup` calls this once per Beethoven-Hardware install.
///
/// We pass the task as a single argv so sbt's command parser
/// receives it as one task — same idiom as `run` above.
pub fn publish_local(project_root: &Path) -> Result<()> {
    let mut cmd = Command::new("sbt");
    cmd.arg("publishLocal").current_dir(project_root);
    exec::run(&mut cmd)?;
    Ok(())
}
