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
