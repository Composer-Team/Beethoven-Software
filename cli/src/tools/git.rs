//! Thin wrappers around the `git` binary. Just enough for `setup` to
//! fetch a fresh clone and pin a ref — no fetch, stash, or pull-ff,
//! because update is implemented as uninstall + reinstall (re-clone),
//! not in-place fast-forward.

use crate::core::exec;
use crate::error::Result;
use std::path::Path;
use std::process::Command;

/// `git clone <url> <dest>`. Full history (not shallow) — same as
/// what users would do by hand, leaves no surprise if they later cd
/// into the dir.
pub fn clone(url: &str, dest: &Path) -> Result<()> {
    let mut cmd = Command::new("git");
    cmd.arg("clone").arg(url).arg(dest);
    exec::run(&mut cmd)?;
    Ok(())
}

/// `git -C <repo> checkout <ref>`. Used to pin a fresh clone to a
/// specific branch / tag / commit.
pub fn checkout(repo: &Path, git_ref: &str) -> Result<()> {
    let mut cmd = Command::new("git");
    cmd.arg("-C").arg(repo).arg("checkout").arg(git_ref);
    exec::run(&mut cmd)?;
    Ok(())
}

/// `git -C <repo> fetch --all --tags`. Used by the hardware-install
/// path before checking out a ref, so a long-lived CLI-managed clone
/// can pick up upstream changes.
pub fn fetch_all(repo: &Path) -> Result<()> {
    let mut cmd = Command::new("git");
    cmd.arg("-C")
        .arg(repo)
        .arg("fetch")
        .arg("--all")
        .arg("--tags");
    exec::run(&mut cmd)?;
    Ok(())
}

/// `git -C <repo> rev-parse --abbrev-ref HEAD` — name of the
/// currently-checked-out branch (e.g. "master"). Used to record what
/// `setup` actually landed on when the user didn't pass `--ref`, so
/// `info` and future `update`s know what's tracked.
pub fn current_branch(repo: &Path) -> Result<String> {
    let out = Command::new("git")
        .arg("-C")
        .arg(repo)
        .arg("rev-parse")
        .arg("--abbrev-ref")
        .arg("HEAD")
        .output()
        .map_err(|e| anyhow::anyhow!("git rev-parse: {e}"))?;
    if !out.status.success() {
        return Err(anyhow::anyhow!(
            "git rev-parse failed: {}",
            String::from_utf8_lossy(&out.stderr).trim()
        )
        .into());
    }
    Ok(String::from_utf8_lossy(&out.stdout).trim().to_string())
}

/// True iff `git_ref` resolves to a commit in `repo`. Used as a
/// guard before `checkout` so a stale `ref = "main"` lingering in
/// user config (from before the master/main rename was settled)
/// silently falls back to HEAD instead of erroring out.
pub fn ref_exists(repo: &Path, git_ref: &str) -> bool {
    Command::new("git")
        .arg("-C")
        .arg(repo)
        .arg("rev-parse")
        .arg("--verify")
        .arg("--quiet")
        .arg(format!("{git_ref}^{{commit}}"))
        .output()
        .map(|out| out.status.success())
        .unwrap_or(false)
}
