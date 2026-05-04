//! cmake invocation + manifest-driven uninstall + post-install
//! introspection. We do *not* shell out to `install.sh` /
//! `uninstall.sh` — those scripts are reference-only and slated for
//! removal.
//!
//! Tracking installed files: cmake produces `install_manifest.txt` in
//! the build dir on every `cmake --install`. We copy it into
//! `~/.config/beethoven/install_manifest.txt` so a later `uninstall`
//! can read it, even though the build dir itself is ephemeral (it
//! lives in a tempdir that gets dropped after install).
//!
//! Tracking the cmake user-package-registry: `libbeethoven`'s install
//! rule writes a breadcrumb at `~/.cmake/packages/beethoven/<md5>`
//! (see `libbeethoven/platforms/host/CMakeLists.txt`). That file isn't
//! captured in `install_manifest.txt` because cmake's `install(CODE
//! ...)` block writes it via `file(WRITE)`, not `install()`. We sweep
//! these by content-matching the prefix path — no md5 reimplementation
//! needed.

use crate::core::exec;
use crate::error::{CliError, Result};
use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

// ---------- Install pipeline (cmake configure + build + install) ----------

/// `cmake -S <source> -B <build> [-D<k>=<v> ...]`. Creates `<build>`
/// if needed. Caller supplies the full set of `-D` flags — this is
/// reused for libbeethoven install (CMAKE_INSTALL_PREFIX +
/// CMAKE_BUILD_TYPE), the runtime build (BEETHOVEN_PROJECT_ROOT etc.),
/// and the user-sw build (BEETHOVEN_PROJECT_ROOT, BEETHOVEN_PLATFORM).
pub fn configure(source: &Path, build: &Path, defines: &[(&str, &str)]) -> Result<()> {
    fs::create_dir_all(build).map_err(|e| {
        CliError::config(format!(
            "cannot create build dir {}: {e}",
            build.display()
        ))
    })?;
    let mut cmd = Command::new("cmake");
    cmd.arg("-S").arg(source).arg("-B").arg(build);
    for (k, v) in defines {
        cmd.arg(format!("-D{k}={v}"));
    }
    exec::run(&mut cmd)?;
    Ok(())
}

/// `cmake --build <build> -j[N]`. With no `jobs`, passes a bare `-j`
/// which cmake interprets as "use all available cores."
pub fn build(build: &Path, jobs: Option<usize>) -> Result<()> {
    let mut cmd = Command::new("cmake");
    cmd.arg("--build").arg(build);
    if let Some(j) = jobs {
        cmd.arg(format!("-j{j}"));
    } else {
        cmd.arg("-j");
    }
    exec::run(&mut cmd)?;
    Ok(())
}

/// `cmake --install <build>`. Writes `install_manifest.txt` in the
/// build dir as a side-effect.
pub fn install(build: &Path) -> Result<()> {
    let mut cmd = Command::new("cmake");
    cmd.arg("--install").arg(build);
    exec::run(&mut cmd)?;
    Ok(())
}

/// Copy `<build>/install_manifest.txt` to `dst`, creating parent dirs
/// as needed. The destination is the persistent manifest under the
/// user config dir.
pub fn copy_manifest(build: &Path, dst: &Path) -> Result<()> {
    let src = build.join("install_manifest.txt");
    if let Some(parent) = dst.parent() {
        fs::create_dir_all(parent).map_err(|e| {
            CliError::config(format!(
                "cannot create manifest dir {}: {e}",
                parent.display()
            ))
        })?;
    }
    fs::copy(&src, dst).map_err(|e| {
        CliError::config(format!(
            "cannot copy install manifest from {} to {}: {e}",
            src.display(),
            dst.display()
        ))
    })?;
    Ok(())
}

// ---------- Uninstall (manifest-driven) ----------

/// Read the persisted install manifest and remove every listed file.
/// Then sweep parent directories rooted at the prefix in deepest-first
/// order — `rmdir` succeeds only on empty dirs, so dirs shared with
/// other packages are left alone automatically.
pub fn uninstall_from_manifest(manifest: &Path, prefix: &Path) -> Result<()> {
    let body = fs::read_to_string(manifest).map_err(|e| {
        CliError::config(format!(
            "cannot read manifest at {}: {e}",
            manifest.display()
        ))
    })?;

    let mut dirs_to_sweep: BTreeSet<PathBuf> = BTreeSet::new();

    for line in body.lines() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        let path = Path::new(line);

        // Best-effort remove. ENOENT is fine — partial uninstalls
        // (or two `uninstall` runs) shouldn't error.
        match fs::remove_file(path) {
            Ok(()) => {}
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
            Err(e) => {
                tracing::warn!("could not remove {}: {e}", path.display());
            }
        }

        // Walk parents up to (but not including) the prefix; we don't
        // want to rmdir the prefix itself, only Beethoven-owned
        // subdirs inside it.
        let mut p = path.parent();
        while let Some(parent) = p {
            if parent == prefix || !parent.starts_with(prefix) {
                break;
            }
            dirs_to_sweep.insert(parent.to_path_buf());
            p = parent.parent();
        }
    }

    // Deepest-first so children are removed before their parents.
    let mut dirs: Vec<_> = dirs_to_sweep.into_iter().collect();
    dirs.sort_by_key(|p| std::cmp::Reverse(p.components().count()));
    for dir in dirs {
        // Ignore errors: non-empty (= shared with another package) is
        // expected and fine.
        let _ = fs::remove_dir(&dir);
    }

    Ok(())
}

/// Sweep `~/.cmake/packages/beethoven/` and remove entries whose
/// contents point inside `prefix`. Each entry is a single-line file
/// containing the cmake-config-package directory; entries created by
/// our install will start with `<prefix>/`.
pub fn remove_registry_entries_for(prefix: &Path) -> Result<()> {
    let home = match dirs::home_dir() {
        Some(h) => h,
        None => return Ok(()),
    };
    let registry_dir = home.join(".cmake/packages/beethoven");
    if !registry_dir.is_dir() {
        return Ok(());
    }

    for entry in fs::read_dir(&registry_dir).map_err(|e| {
        CliError::config(format!("cannot read {}: {e}", registry_dir.display()))
    })? {
        let entry = match entry {
            Ok(e) => e,
            Err(_) => continue,
        };
        let path = entry.path();
        let content = match fs::read_to_string(&path) {
            Ok(c) => c,
            Err(_) => continue,
        };
        let target = Path::new(content.trim());
        if target.starts_with(prefix) {
            let _ = fs::remove_file(&path);
        }
    }

    // If we just emptied the registry dir for this package, tidy up.
    let _ = fs::remove_dir(&registry_dir);

    Ok(())
}

// ---------- Post-install introspection (used by `info`) ----------

/// Read the persisted install manifest and return the subset of paths
/// that look like `libbeethoven-*.{so,dylib,a}`. Best-effort: unreadable
/// manifest → empty vec.
pub fn find_installed_libs(manifest: &Path) -> Vec<PathBuf> {
    let body = match fs::read_to_string(manifest) {
        Ok(b) => b,
        Err(_) => return Vec::new(),
    };
    body.lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty())
        .filter_map(|l| {
            let path = Path::new(l);
            let name = path.file_name()?.to_str()?;
            if name.starts_with("libbeethoven-")
                && (name.ends_with(".so") || name.ends_with(".dylib") || name.ends_with(".a"))
            {
                Some(path.to_path_buf())
            } else {
                None
            }
        })
        .collect()
}

/// Locate `BEETHOVEN_RUNTIME_SRC_DIR` for a libbeethoven install.
///
/// Fast path: the documented install location at
/// `<prefix>/share/beethoven/runtime-src/` — works on every distro
/// where `GNUInstallDirs::CMAKE_INSTALL_DATAROOTDIR` resolves to
/// `share`, which is essentially all of them.
///
/// Slow path (only if the fast path doesn't exist): parse
/// `beethovenConfig.cmake`. Handles both `set(KEY ...)` and
/// `set_and_check(KEY ...)`, and substitutes the cmake variable
/// `${PACKAGE_PREFIX_DIR}` with the install prefix (cmake's config
/// generator emits a path of the form
/// `${PACKAGE_PREFIX_DIR}/share/beethoven/runtime-src`).
pub fn parse_runtime_src_dir(prefix: &Path) -> Option<PathBuf> {
    // Fast path: the documented default.
    let canonical = prefix.join("share").join("beethoven").join("runtime-src");
    if canonical.is_dir() {
        return Some(canonical);
    }

    // Slow path: parse the installed cmake config.
    for libdir in &["lib", "lib64", "lib/x86_64-linux-gnu"] {
        let cfg = prefix
            .join(libdir)
            .join("cmake")
            .join("beethoven")
            .join("beethovenConfig.cmake");
        if let Ok(body) = fs::read_to_string(&cfg) {
            if let Some(path) = extract_runtime_src(&body, prefix) {
                if path.is_dir() {
                    return Some(path);
                }
            }
        }
    }
    None
}

/// Pull `BEETHOVEN_RUNTIME_SRC_DIR` out of a cmake config body.
/// Accepts both `set(...)` and `set_and_check(...)` (cmake's
/// `configure_package_config_file` emits the latter when the
/// template uses `set_and_check`). Substitutes
/// `${PACKAGE_PREFIX_DIR}` — defined by the cmake config as
/// `${CMAKE_CURRENT_LIST_DIR}/../../../`, i.e. the install prefix —
/// with the actual prefix path so we get an absolute, usable result.
fn extract_runtime_src(body: &str, prefix: &Path) -> Option<PathBuf> {
    const KEY: &str = "BEETHOVEN_RUNTIME_SRC_DIR";
    for line in body.lines() {
        let line = line.trim();
        let rest = if let Some(r) = line.strip_prefix("set_and_check(") {
            r
        } else if let Some(r) = line.strip_prefix("set(") {
            r
        } else {
            continue;
        };
        let rest = rest.trim_start();
        let after_key = match rest.strip_prefix(KEY) {
            Some(r) => r.trim_start(),
            None => continue,
        };
        let q1 = after_key.find('"')?;
        let after_q1 = &after_key[q1 + 1..];
        let q2 = after_q1.find('"')?;
        let raw = &after_q1[..q2];
        // Substitute the one cmake variable the config sets locally.
        let resolved = raw.replace("${PACKAGE_PREFIX_DIR}", &prefix.display().to_string());
        return Some(PathBuf::from(resolved));
    }
    None
}
