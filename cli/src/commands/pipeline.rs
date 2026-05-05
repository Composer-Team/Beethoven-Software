//! Shared end-to-end pipeline used by `sim` and `run`.
//!
//! Probe → auto-build (only sw if a daemon is already up; full
//! hw + runtime + sw if not) → launch daemon if Down → exec testbench
//! → teardown if we launched. `sim` and `run` are thin wrappers that
//! supply the right mode + target validation.
//!
//! Lives in `commands/` (not `runtime/`) because it imports from
//! `commands::build` — keeping the layer rule one-way.

use crate::commands::build;
use crate::error::{CliError, Result};
use crate::runtime::lifecycle;
use crate::runtime::probe::{self, ProbeResult};
use crate::state::Project;
use crate::ui;
use std::fs;
use std::os::unix::fs::PermissionsExt;
use std::path::PathBuf;
use std::process::{Child, Command};
use std::time::Duration;

/// Time we'll wait for a freshly-spawned daemon to acquire its lock
/// before declaring it stuck. Verilator/icarus first-time elaboration
/// can take a while; 60s is comfortable headroom.
const READY_TIMEOUT: Duration = Duration::from_secs(60);

pub(crate) fn execute(
    project: &Project,
    mode: &str,
    no_build: bool,
    no_launch: bool,
    testbench_arg: Option<&str>,
    simulator_override: Option<&str>,
    tb_args: &[String],
) -> Result<()> {
    // 1. Probe daemon
    let daemon_state = probe::probe(&project.root)?;
    let daemon_was_up = matches!(daemon_state, ProbeResult::Up { .. });
    match &daemon_state {
        ProbeResult::Up { pid } => {
            let pid_str = pid.map_or("unknown".into(), |p| p.to_string());
            ui::print_note(&format!("reusing existing daemon (PID {pid_str})"));
        }
        ProbeResult::Error(e) => {
            ui::print_warning(&format!("daemon probe failed: {e}; assuming Down"));
        }
        ProbeResult::Down => {}
    }

    // 2. Auto-build (skip per-layer when artifacts are already present;
    // user can force rebuild via `beethoven build [hw|runtime|sw]` or
    // wipe state with `beethoven clean` / `runtime clean`).
    if !no_build {
        if daemon_was_up {
            // Daemon is up — only sw is rebuildable on the fly. Hw and
            // runtime changes need a daemon restart to take effect, so
            // silently rebuilding them would be misleading.
            if sw_already_built(project) {
                ui::print_stage("Skipped", "sw — testbench already built");
            } else {
                build::build_sw(project, None)?;
            }
        } else {
            if build::hw_already_built(project, mode) {
                ui::print_stage(
                    "Skipped",
                    &format!("hw — bindings + {mode}/hw/ already present"),
                );
            } else {
                build::build_hw(project, mode)?;
            }
            // Always invoke `build_runtime`; cmake's dep system is the
            // source of truth for incrementality. A presence-only skip
            // here masked stale daemons when libbeethoven was reinstalled
            // with a new source file (e.g. singleton_lock.cc) but the
            // prior `.vpi`/binary was still on disk.
            build::build_runtime(project, mode, None, simulator_override)?;
            if sw_already_built(project) {
                ui::print_stage("Skipped", "sw — testbench already built");
            } else {
                build::build_sw(project, None)?;
            }
        }
    }

    // 3. Launch daemon if Down
    let mut launched: Option<Child> = None;
    if !daemon_was_up {
        if no_launch {
            return Err(CliError::config(
                "--no-launch was set but no daemon is running for this project. \
                 Start one with `beethoven runtime run`."
                    .to_string(),
            ));
        }
        let mut cmd = lifecycle::build_launch_command(project, mode)?;
        cmd.env("BEETHOVEN_PROJECT_ROOT", &project.root);
        // Daemon dumps waveforms (trace.vcd / trace.fst /
        // BeethovenTrace.vpd) and DRAMsim3 logs into its cwd. Pin it to
        // target/<mode>/ so those land alongside other build artifacts
        // instead of polluting wherever the user invoked the CLI.
        let dump_dir = project.root.join("target").join(mode);
        fs::create_dir_all(&dump_dir).map_err(|e| {
            anyhow::anyhow!("failed to create {}: {e}", dump_dir.display())
        })?;
        cmd.current_dir(&dump_dir);
        ui::print_stage("Starting", &format!("BeethovenRuntime ({mode})"));
        ui::print_exec(&cmd);
        let mut child = cmd
            .spawn()
            .map_err(|e| anyhow::anyhow!("failed to spawn daemon: {e}"))?;
        if let Err(e) = lifecycle::wait_for_ready(project, &mut child, READY_TIMEOUT) {
            // Daemon never became ready; best-effort cleanup.
            let _ = lifecycle::shutdown_daemon(&mut child);
            return Err(e);
        }
        launched = Some(child);
    }

    // 4. Run the testbench. Wrap in a closure so we can guarantee
    // teardown of `launched` even if testbench-spawn fails.
    let testbench_outcome = (|| -> Result<std::process::ExitStatus> {
        let tb_path = select_testbench(project, testbench_arg)?;
        let tb_name = tb_path
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("testbench")
            .to_string();
        ui::print_stage("Running", &tb_name);
        let mut tb_cmd = Command::new(&tb_path);
        tb_cmd.env("BEETHOVEN_PROJECT_ROOT", &project.root);
        tb_cmd.args(tb_args);
        ui::print_exec(&tb_cmd);
        tb_cmd
            .status()
            .map_err(|e| anyhow::anyhow!("failed to spawn testbench: {e}").into())
    })();

    // 5. Teardown if we launched the daemon. Always runs, regardless
    // of testbench outcome.
    if let Some(mut child) = launched {
        ui::print_stage("Stopping", "BeethovenRuntime");
        let _ = lifecycle::shutdown_daemon(&mut child);
    }

    // 6. Propagate testbench result.
    let status = testbench_outcome?;
    if status.success() {
        ui::print_success("done.");
        Ok(())
    } else {
        Err(anyhow::anyhow!("testbench exited with {status}").into())
    }
}

// ---- testbench discovery ----

/// True if `target/sw/` contains at least one built testbench binary.
/// Used by `execute` to decide whether to skip the sw build step when
/// auto-building for `sim`/`run`.
fn sw_already_built(project: &Project) -> bool {
    find_testbenches(project).map_or(false, |v| !v.is_empty())
}

/// Scan `target/sw/` for executable files, skipping cmake bookkeeping.
fn find_testbenches(project: &Project) -> Result<Vec<PathBuf>> {
    let sw_dir = project.root.join("target").join("sw");
    if !sw_dir.is_dir() {
        return Err(CliError::config(format!(
            "no testbench directory at {}; run `beethoven build sw` first",
            sw_dir.display()
        )));
    }
    let mut tbs = Vec::new();
    for entry in fs::read_dir(&sw_dir)? {
        let entry = entry?;
        let path = entry.path();
        if !path.is_file() {
            continue;
        }
        let name = match path.file_name().and_then(|n| n.to_str()) {
            Some(n) => n,
            None => continue,
        };
        if name.starts_with('.')
            || name.ends_with(".cmake")
            || name == "Makefile"
            || name == "CMakeCache.txt"
        {
            continue;
        }
        let mode = entry.metadata()?.permissions().mode();
        if mode & 0o111 == 0 {
            continue;
        }
        tbs.push(path);
    }
    Ok(tbs)
}

fn select_testbench(project: &Project, name: Option<&str>) -> Result<PathBuf> {
    let tbs = find_testbenches(project)?;
    if tbs.is_empty() {
        return Err(CliError::config(format!(
            "no testbench binaries found in {}/target/sw/. Run `beethoven build sw` first.",
            project.root.display()
        )));
    }
    if let Some(want) = name {
        for tb in &tbs {
            if tb.file_name().and_then(|n| n.to_str()) == Some(want) {
                return Ok(tb.clone());
            }
        }
        let names: Vec<&str> = tbs
            .iter()
            .filter_map(|p| p.file_name().and_then(|n| n.to_str()))
            .collect();
        return Err(CliError::config(format!(
            "testbench '{want}' not found. Available: {}",
            names.join(", ")
        )));
    }
    if tbs.len() == 1 {
        return Ok(tbs[0].clone());
    }
    let names: Vec<&str> = tbs
        .iter()
        .filter_map(|p| p.file_name().and_then(|n| n.to_str()))
        .collect();
    Err(CliError::config(format!(
        "multiple testbenches available ({}); specify one as a positional arg",
        names.join(", ")
    )))
}
