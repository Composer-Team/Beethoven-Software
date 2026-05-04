//! BeethovenRuntime daemon lifecycle helpers.
//!
//! `build_launch_command` dispatches on `(mode, simulator)` to produce
//! the right `Command` for the backend's artifacts:
//!
//!   - verilator (sim) / synthesis: exec `<out>/BeethovenRuntime`
//!   - icarus (sim): `vvp -M <out> -m sim_BeethovenRuntime
//!     <out>/beethoven.vvp` (vvp loads the VPI plugin which IS the
//!     daemon code — one process, not two)
//!   - vcs (sim): exec `<out>/BeethovenTop`
//!
//! `report_exit` formats the daemon's exit status with cargo-style
//! prefixes — clean exit vs SIGINT vs other signals get different
//! treatment so users can tell "I hit Ctrl+C" from "the daemon
//! crashed."

use crate::core::exec;
use crate::error::{CliError, Result};
use crate::runtime::probe::{self, ProbeResult};
use crate::state::Project;
use crate::ui;
use std::os::unix::process::ExitStatusExt;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, ExitStatus};
use std::thread;
use std::time::{Duration, Instant};

/// Build the `Command` that launches the daemon for `mode`. Verifies
/// all required artifacts exist before returning; errors with a
/// `Run \`beethoven build runtime\` first` hint if any are missing.
/// Required tools (e.g. `vvp` for icarus) are also checked here.
pub fn build_launch_command(project: &Project, mode: &str) -> Result<Command> {
    let out = runtime_out_dir(project, mode);

    if mode == "synthesis" {
        let bin = out.join("BeethovenRuntime");
        verify_exists(&bin, "synthesis daemon")?;
        return Ok(Command::new(bin));
    }

    // Simulation: dispatch on the simulator backend.
    match project.simulator() {
        "verilator" => {
            let bin = out.join("BeethovenRuntime");
            verify_exists(&bin, "verilator daemon")?;
            Ok(Command::new(bin))
        }
        "icarus" => {
            let vvp = exec::require_tool(
                "vvp",
                Some("install Icarus Verilog (iverilog package)"),
            )?;
            let vvp_file = out.join("beethoven.vvp");
            let vpi_file = out.join("sim_BeethovenRuntime.vpi");
            verify_exists(&vvp_file, "icarus vvp")?;
            verify_exists(&vpi_file, "icarus VPI plugin")?;
            let mut cmd = Command::new(vvp);
            cmd.arg("-M")
                .arg(&out)
                .arg("-m")
                .arg("sim_BeethovenRuntime")
                .arg(&vvp_file);
            Ok(cmd)
        }
        "vcs" => {
            let bin = out.join("BeethovenTop");
            verify_exists(&bin, "vcs binary")?;
            Ok(Command::new(bin))
        }
        other => Err(CliError::config(format!(
            "unknown simulator '{other}' in [platform].simulator \
             (expected: verilator, icarus, vcs)"
        ))),
    }
}

/// `<project>/target/<mode>/runtime/`.
pub fn runtime_out_dir(project: &Project, mode: &str) -> PathBuf {
    project.root.join("target").join(mode).join("runtime")
}

fn verify_exists(p: &Path, what: &str) -> Result<()> {
    if !p.exists() {
        return Err(CliError::config(format!(
            "missing {what} artifact at {}.\n\
             Run `beethoven build runtime` first.",
            p.display()
        )));
    }
    Ok(())
}

/// Wait for a freshly-spawned daemon to finish startup. We're "ready"
/// when the daemon has acquired its singleton flock — that's our
/// observable proxy for "shmem is mapped and the cmd loop is alive".
///
/// We also poll `child.try_wait` so a daemon that crashes during
/// startup surfaces as a clear error rather than a 60-second timeout.
///
/// After flock acquisition we sleep briefly: the daemon's data-server
/// thread does `shm_open` + `ftruncate` + mutex init a few
/// microseconds *after* the lock is taken, and a testbench launched
/// immediately would race the shmem creation. 200ms is enough margin
/// without being noticeable.
pub fn wait_for_ready(
    project: &Project,
    child: &mut Child,
    timeout: Duration,
) -> Result<()> {
    let start = Instant::now();
    loop {
        if let Some(status) = child
            .try_wait()
            .map_err(|e| anyhow::anyhow!("waiting for daemon failed: {e}"))?
        {
            return Err(CliError::config(format!(
                "daemon exited before becoming ready: {status}"
            )));
        }
        if matches!(probe::probe(&project.root)?, ProbeResult::Up { .. }) {
            // Tiny grace window for shmem init (see doc comment above).
            thread::sleep(Duration::from_millis(200));
            return Ok(());
        }
        if start.elapsed() > timeout {
            return Err(CliError::config(format!(
                "daemon failed to become ready within {}s",
                timeout.as_secs()
            )));
        }
        thread::sleep(Duration::from_millis(100));
    }
}

/// Politely stop a daemon we spawned: SIGTERM, wait up to 5 seconds,
/// then escalate to SIGKILL. Best-effort — errors are swallowed
/// because the worst case (daemon already dead) is fine.
pub fn shutdown_daemon(child: &mut Child) -> Result<()> {
    use nix::sys::signal::{kill, Signal};
    use nix::unistd::Pid;

    let pid = Pid::from_raw(child.id() as i32);
    let _ = kill(pid, Signal::SIGTERM);

    let deadline = Instant::now() + Duration::from_secs(5);
    while Instant::now() < deadline {
        if let Ok(Some(_)) = child.try_wait() {
            return Ok(());
        }
        thread::sleep(Duration::from_millis(100));
    }

    let _ = kill(pid, Signal::SIGKILL);
    let _ = child.wait();
    Ok(())
}

/// Print a cargo-style exit summary. Returns `true` if the exit was
/// "clean enough to count as success" — clean exit (code 0) or
/// user-initiated termination (SIGINT / SIGTERM). The caller uses the
/// return value to decide whether to surface the exit as a CLI error.
pub fn report_exit(status: &ExitStatus) -> bool {
    if status.success() {
        ui::print_success("daemon exited cleanly.");
        return true;
    }
    if let Some(sig) = status.signal() {
        match sig {
            // SIGINT (Ctrl+C) and SIGTERM are user-driven; not failures.
            2 => {
                ui::print_note("daemon stopped (Ctrl+C).");
                true
            }
            15 => {
                ui::print_note("daemon stopped (SIGTERM).");
                true
            }
            other => {
                ui::print_warning(&format!("daemon killed by signal {other}."));
                false
            }
        }
    } else if let Some(code) = status.code() {
        ui::print_warning(&format!("daemon exited with code {code}."));
        false
    } else {
        ui::print_warning("daemon exited with unknown status.");
        false
    }
}
