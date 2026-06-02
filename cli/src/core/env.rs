//! Path resolution shared across subcommands. Mirrors the conventions
//! used by the daemon's `runtime/src/core/singleton_lock.cc` so the
//! CLI's probe and the daemon's lock acquisition resolve to the same
//! lockfile path.

use crate::error::{CliError, Result};
use std::env;
use std::path::{Path, PathBuf};

/// Directory the daemon lockfile lives in. Tries `$XDG_RUNTIME_DIR`
/// (Linux), then `$TMPDIR` (macOS), then `/tmp`. Same fallback chain
/// as `singleton_lock.cc::runtime_dir`.
pub fn runtime_dir() -> PathBuf {
    for var in &["XDG_RUNTIME_DIR", "TMPDIR"] {
        if let Ok(v) = env::var(var) {
            if !v.is_empty() {
                return PathBuf::from(v);
            }
        }
    }
    PathBuf::from("/tmp")
}

/// Effective UID — embedded in the lockfile name so /tmp is safe
/// across users on the same machine.
pub fn effective_uid() -> u32 {
    nix::unistd::geteuid().as_raw()
}

/// FNV-1a 64-bit hash, formatted as 16 hex chars. Mirrors
/// `singleton_lock.cc::fnv1a64` exactly so both sides agree on the
/// project key.
pub fn fnv1a64_hex(s: &[u8]) -> String {
    let mut h: u64 = 14_695_981_039_346_656_037;
    for &b in s {
        h ^= b as u64;
        h = h.wrapping_mul(1_099_511_628_211);
    }
    format!("{h:016x}")
}

/// Per-project key: FNV-1a of the canonicalized project root.
pub fn project_key(project_root: &Path) -> Result<String> {
    let canonical = project_root.canonicalize().map_err(|e| {
        CliError::config(format!(
            "cannot canonicalize project root '{}': {e}",
            project_root.display()
        ))
    })?;
    Ok(fnv1a64_hex(canonical.to_string_lossy().as_bytes()))
}

/// Absolute lockfile path: `<run-dir>/beethoven-<uid>-<key>.lock`.
pub fn lockfile_path(project_root: &Path) -> Result<PathBuf> {
    let key = project_key(project_root)?;
    Ok(runtime_dir().join(format!("beethoven-{}-{}.lock", effective_uid(), key)))
}

/// CLI-managed Beethoven-Hardware checkout. `setup` clones into here
/// (or pulls if it already exists) and runs `sbt publishLocal` from
/// it. Lives next to `~/.local/share/beethoven/runtime-src/` so the
/// two source trees the CLI manages are colocated.
pub fn hardware_src_dir() -> PathBuf {
    dirs::data_dir()
        .unwrap_or_else(|| dirs::home_dir().unwrap_or_default().join(".local/share"))
        .join("beethoven")
        .join("hardware-src")
}

/// Default install prefix — `~/.local`. Used by `setup` when no
/// `--prefix` flag is given and the user config has none yet.
pub fn default_prefix() -> PathBuf {
    dirs::home_dir()
        .unwrap_or_else(|| PathBuf::from("/"))
        .join(".local")
}

/// Expand a leading `~/` to the user's home directory. If the path
/// doesn't start with `~/`, returns it as-is. Also handles bare `~`.
pub fn expand_tilde(p: &Path) -> PathBuf {
    let s = p.to_string_lossy();
    if s == "~" {
        return dirs::home_dir().unwrap_or_else(|| p.to_path_buf());
    }
    if let Some(stripped) = s.strip_prefix("~/") {
        if let Some(home) = dirs::home_dir() {
            return home.join(stripped);
        }
    }
    p.to_path_buf()
}

/// Path to the persisted install manifest:
/// `~/.config/beethoven/install_manifest.txt`. Written by `setup`
/// (copied from cmake's build-dir manifest), consumed by `uninstall`.
pub fn manifest_path() -> Result<PathBuf> {
    let dir = dirs::config_dir().ok_or_else(|| {
        CliError::config("cannot resolve config dir; set $XDG_CONFIG_HOME or $HOME")
    })?;
    Ok(dir.join("beethoven").join("install_manifest.txt"))
}

/// CLI cache dir: `~/.cache/beethoven/` (XDG-aware). Currently only
/// touched by `uninstall` (to wipe any leftover state); the
/// long-running CLI doesn't accumulate anything here in normal flow.
pub fn cache_dir() -> PathBuf {
    dirs::cache_dir()
        .unwrap_or_else(|| dirs::home_dir().unwrap_or_default().join(".cache"))
        .join("beethoven")
}
