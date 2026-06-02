//! `beethoven runtime <build|run|clean>` — daemon-focused subset of
//! the build/run pipeline.
//!
//! - `Build` and `Clean` reuse `commands::build` so there's a single
//!   source of truth for "how to build the runtime."
//! - `Run` launches the (already-built) daemon in the foreground via
//!   `runtime::lifecycle::build_launch_command`; refuses if a daemon
//!   is already up for this project (probe via `runtime::probe`).

use crate::cli::{RuntimeCommand, RuntimeKillArgs, RuntimeRunArgs};
use crate::commands::build;
use crate::core::env;
use crate::error::{CliError, Result};
use crate::runtime::lifecycle::{self, KillOutcome};
use crate::runtime::probe::{self, ProbeResult};
use crate::state::Project;
use crate::ui;
use std::fs;
use std::path::Path;

pub fn run(cmd: RuntimeCommand) -> Result<()> {
    match cmd {
        RuntimeCommand::Build { release, jobs } => {
            let project = Project::discover()?;
            let mode = if release { "synthesis" } else { "simulation" };
            build::build_runtime(&project, mode, jobs, None)?;
            ui::print_success("runtime built.");
            Ok(())
        }
        RuntimeCommand::Run(args) => run_daemon(args),
        RuntimeCommand::Kill(args) => kill_daemon(args),
        RuntimeCommand::Clean { release } => clean_runtime(release),
    }
}

/// SIGKILL the daemon for this project, then wipe per-project +
/// per-user runtime state (lockfile, shmem, both modes' cmake build
/// dirs). Idempotent: with no daemon running, the kill phase is a
/// no-op note and we still run the cleanup pass.
///
/// We resolve the daemon via the lockfile (probe → PID), not via
/// pgrep/ps, so the implementation is identical on Linux and macOS.
fn kill_daemon(_args: RuntimeKillArgs) -> Result<()> {
    let project = Project::discover()?;

    match probe::probe(&project.root)? {
        ProbeResult::Down => {
            ui::print_note("no BeethovenRuntime running for this project.");
        }
        ProbeResult::Up { pid: Some(pid) } => {
            ui::print_stage("Killing", &format!("BeethovenRuntime (PID {pid}, SIGKILL)"));
            match lifecycle::kill_external_daemon(&project, pid)? {
                KillOutcome::AlreadyGone => {
                    ui::print_note(&format!(
                        "daemon (PID {pid}) was already gone before the signal landed."
                    ));
                }
                KillOutcome::Killed => {
                    ui::print_success(&format!("daemon (PID {pid}) killed."));
                }
            }
        }
        ProbeResult::Up { pid: None } => {
            // Lockfile is held but contains no PID. Daemon raced its
            // pwrite, or someone truncated the file. We can't signal
            // without a PID; punt with a clear hint and DON'T wipe
            // state — there's a daemon out there with shmem mapped.
            return Err(CliError::config(
                "daemon is running but its PID could not be read from the lockfile. \
                 Stop it via the terminal that launched it, or `pkill BeethovenRuntime`."
                    .to_string(),
            ));
        }
        ProbeResult::Error(e) => {
            return Err(CliError::config(format!("daemon probe failed: {e}")));
        }
    }

    // Auto-clean. Both modes — we don't track which mode the killed
    // daemon was running in, and removing the inactive mode's dir is
    // harmless. Lockfile + shmem are mode-independent and only
    // wiped once.
    wipe_runtime_state(&project, &["simulation", "synthesis"])
}

/// Foreground daemon launch. Refuses if another daemon is already up
/// for this project; otherwise spawns and waits, propagating exit
/// status.
fn run_daemon(args: RuntimeRunArgs) -> Result<()> {
    let project = Project::discover()?;
    let mode = if args.release {
        "synthesis"
    } else {
        "simulation"
    };

    // Refuse double-start. Probe is non-blocking and microseconds.
    match probe::probe(&project.root)? {
        ProbeResult::Up { pid } => {
            let pid_str = pid.map_or("unknown".into(), |p| p.to_string());
            return Err(CliError::config(format!(
                "BeethovenRuntime is already running for this project (PID {pid_str}). \
                 Stop it first (Ctrl+C in its terminal, or `kill {pid_str}`)."
            )));
        }
        ProbeResult::Error(e) => {
            // Non-fatal — probe could fail on weird filesystems. Log
            // and proceed; the daemon's own flock acquisition is the
            // authoritative singleton check.
            ui::print_warning(&format!("daemon probe failed: {e}; proceeding anyway"));
        }
        ProbeResult::Down => {}
    }

    let mut cmd = lifecycle::build_launch_command(&project, mode)?;
    // Daemon's singleton_lock.cc reads BEETHOVEN_PROJECT_ROOT to
    // compute the project key for the flock. We MUST set this — the
    // daemon will refuse to start otherwise.
    cmd.env("BEETHOVEN_PROJECT_ROOT", &project.root);

    // --log-file: redirect stderr to a file (no tee). Stdout always
    // inherits so the user can see the daemon's "ready" / progress
    // output in their terminal.
    if let Some(log_path) = &args.log_file {
        let f = std::fs::File::create(log_path).map_err(|e| {
            CliError::config(format!("cannot open log file {}: {e}", log_path.display()))
        })?;
        cmd.stderr(std::process::Stdio::from(f));
    }

    ui::print_stage(
        "Starting",
        &format!("BeethovenRuntime ({mode}); Ctrl+C to stop"),
    );
    ui::print_exec(&cmd);
    let mut child = cmd
        .spawn()
        .map_err(|e| anyhow::anyhow!("failed to spawn daemon: {e}"))?;
    let pid = child.id();
    tracing::info!("daemon spawned with PID {pid}");

    // Wait. We deliberately don't install our own SIGINT handler:
    // Ctrl+C goes to the foreground process group, both this CLI and
    // the daemon get SIGINT, the daemon exits, our wait returns.
    // Standard "cargo run" pattern.
    let status = child
        .wait()
        .map_err(|e| anyhow::anyhow!("waiting for daemon failed: {e}"))?;

    if lifecycle::report_exit(&status) {
        Ok(())
    } else {
        Err(anyhow::anyhow!("daemon exited with {status}").into())
    }
}

/// `runtime clean` — wipe everything that would interfere with a
/// fresh daemon launch:
///   - `target/<mode>/runtime/` (the cmake build dir)
///   - the per-project flock lockfile
///   - the per-user POSIX shmem segments (`/compo_c_<uid>`,
///     `/compo_d_<uid>`) — these don't auto-clean on daemon crash,
///     and stale ones are the typical cause of "sim hangs forever"
///     after a Ctrl+C.
///
/// Refuses to run if a daemon is currently up for this project.
/// (`runtime kill` shares the same wiping logic but skips this
/// guard — it's responsible for terminating the daemon first.)
fn clean_runtime(release: bool) -> Result<()> {
    let project = Project::discover()?;
    let mode = if release { "synthesis" } else { "simulation" };

    // Refuse if a live daemon is holding the lock; cleaning shmem
    // out from under it would brick the running testbench.
    if let ProbeResult::Up { pid } = probe::probe(&project.root)? {
        let pid_str = pid.map_or("unknown".into(), |p| p.to_string());
        return Err(CliError::config(format!(
            "BeethovenRuntime is currently running for this project (PID {pid_str}); \
             stop it first."
        )));
    }

    wipe_runtime_state(&project, &[mode])
}

/// Shared wiping pass used by both `runtime clean` and `runtime
/// kill`. Caller is responsible for ensuring no daemon is up; this
/// function will happily nuke shmem out from under a live daemon.
///
/// `modes` is the set of `target/<mode>/runtime/` directories to
/// remove (one per build mode). The lockfile and shmem segments are
/// mode-independent; we touch them exactly once regardless of how
/// many modes are passed in.
fn wipe_runtime_state(project: &Project, modes: &[&str]) -> Result<()> {
    let mut anything_removed = false;

    // 1. cmake build dirs (one per mode).
    for mode in modes {
        let runtime_dir = lifecycle::runtime_out_dir(project, mode);
        if runtime_dir.exists() {
            ui::print_stage("Removing", &runtime_dir.display().to_string());
            fs::remove_dir_all(&runtime_dir)?;
            anything_removed = true;
        }
    }

    // 2. Per-project flock lockfile (flock state is kernel-managed
    // and was released when the holding process died, but the file
    // itself persists with stale PID contents — remove for tidiness).
    let lockfile = env::lockfile_path(&project.root)?;
    if lockfile.exists() {
        ui::print_stage("Removing", &lockfile.display().to_string());
        let _ = fs::remove_file(&lockfile);
        anything_removed = true;
    }

    // 3. Per-user POSIX shmem segments. These are at /dev/shm/...
    // on Linux. Removing them between runs prevents the "stale shmem
    // mutex hangs the next daemon" trap. See
    // runtime/src/core/singleton_lock.cc for the same cleanup on
    // the daemon side.
    let uid = env::effective_uid();
    for which in &["c", "d"] {
        let path_str = format!("/dev/shm/compo_{which}_{uid}");
        let path = Path::new(&path_str);
        if path.exists() {
            ui::print_stage("Removing", &path_str);
            let _ = fs::remove_file(path);
            anything_removed = true;
        }
    }

    if anything_removed {
        ui::print_success("runtime artifacts removed.");
    } else {
        ui::print_note("nothing to clean.");
    }
    Ok(())
}
