//! Probe whether a `BeethovenRuntime` is running for a given project.
//!
//! Mirrors the daemon-side flock logic in
//! `runtime/src/core/singleton_lock.cc`. We open the lockfile and
//! attempt a non-blocking shared `flock`. Success means no daemon is
//! up; `EWOULDBLOCK` means one is, and we read its PID for diagnostics.
//!
//! There is a tiny race window where two probes hold shared locks
//! concurrently and a daemon's exclusive-lock acquisition fails —
//! but the window is microseconds wide and a retry resolves it. Not
//! worth a heavier mechanism for v1.

use crate::core::env;
use crate::error::Result;
use nix::errno::Errno;
use nix::fcntl::{Flock, FlockArg};
use std::fs::{File, OpenOptions};
use std::io::{ErrorKind, Read, Seek, SeekFrom};
use std::path::Path;

#[derive(Debug)]
pub enum ProbeResult {
    /// No daemon is running for this project.
    Down,
    /// A daemon is holding the lock; PID may be unavailable if the
    /// lockfile is empty / unreadable.
    Up { pid: Option<i32> },
    /// Unexpected I/O error opening or locking the file.
    Error(std::io::Error),
}

/// Probe daemon presence for `project_root`.
pub fn probe(project_root: &Path) -> Result<ProbeResult> {
    let path = env::lockfile_path(project_root)?;
    Ok(probe_path(&path))
}

/// Probe a specific lockfile path. Useful for tests.
pub fn probe_path(path: &Path) -> ProbeResult {
    let file = match OpenOptions::new().read(true).open(path) {
        Ok(f) => f,
        // No lockfile = no daemon (or it crashed in such a way that
        // the file was never created — same outcome from our POV).
        Err(e) if e.kind() == ErrorKind::NotFound => return ProbeResult::Down,
        Err(e) => return ProbeResult::Error(e),
    };

    // nix's `Flock::lock` consumes the file. On success it returns a
    // guard that releases on drop (so we get a tiny shared-lock
    // window). On failure it gives the file back so we can read the
    // holding daemon's PID for diagnostics.
    match Flock::lock(file, FlockArg::LockSharedNonblock) {
        Ok(_guard) => ProbeResult::Down,
        Err((mut file, Errno::EWOULDBLOCK)) => {
            let pid = read_pid(&mut file).ok();
            ProbeResult::Up { pid }
        }
        Err((_file, e)) => ProbeResult::Error(std::io::Error::from_raw_os_error(e as i32)),
    }
}

fn read_pid(file: &mut File) -> std::io::Result<i32> {
    file.seek(SeekFrom::Start(0))?;
    let mut buf = String::new();
    file.read_to_string(&mut buf)?;
    buf.trim()
        .parse::<i32>()
        .map_err(|e| std::io::Error::new(ErrorKind::InvalidData, e))
}
