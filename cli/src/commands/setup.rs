//! `beethoven setup` — clone Beethoven-Software (or use a local
//! `--from` path), cmake-build it, install libbeethoven into the
//! user's prefix.
//!
//! Workflow:
//!
//!   1. Resolve prefix / ref / from / jobs from CLI args, falling back
//!      to user config and built-in defaults.
//!   2. Validate `--from` if provided (sanity check it's a clone).
//!   3. Pre-flight: require git (unless --from), cmake, a C++ compiler.
//!   4. Create a scratch tempdir (auto-cleaned via Drop, even on
//!      early bail).
//!   5. Acquire the source tree: clone into scratch (or use `--from`).
//!   6. cmake configure → build → install (build dir lives inside
//!      scratch, so the user's `--from` directory is never written to).
//!   7. Copy `install_manifest.txt` into `~/.config/beethoven/` so
//!      `uninstall` can find it later, since scratch is about to drop.
//!   8. Persist {prefix, ref} to the user config.
//!
//! Future scope: this is also where Scala / sbt installation will
//! land. Keep the structure additive — each install phase is its own
//! function called sequentially from `do_setup`.

use crate::cli::SetupArgs;
use crate::core::{env, exec};
use crate::error::{CliError, Result};
use crate::state::UserConfig;
use crate::tools::{cmake, git};
use crate::ui;
use std::path::{Path, PathBuf};

const REPO_URL: &str = "https://github.com/Composer-Team/Beethoven-Software";

pub fn run(args: SetupArgs) -> Result<()> {
    let cfg = UserConfig::load()?;
    let prefix = resolve_prefix(args.prefix.as_deref(), &cfg);
    let git_ref = args
        .git_ref
        .or_else(|| cfg.git_ref.clone())
        .unwrap_or_else(|| "main".into());

    do_setup(&prefix, &git_ref, args.from.as_deref(), args.jobs)
}

/// The reusable worker. Called by `setup::run` directly and by
/// `update::run` after it runs the uninstall step.
pub fn do_setup(
    prefix: &Path,
    git_ref: &str,
    from: Option<&Path>,
    jobs: Option<usize>,
) -> Result<()> {
    if let Some(p) = from {
        validate_from_path(p)?;
    }
    pre_flight(from)?;

    // tempfile::TempDir auto-removes on Drop, including on `?` early
    // returns from any of the cmake steps below. The user's `--from`
    // directory is *outside* this scratch dir, so it's untouched.
    let scratch = tempfile::Builder::new()
        .prefix("beethoven-setup-")
        .tempdir()
        .map_err(|e| CliError::config(format!("cannot create scratch tempdir: {e}")))?;
    let scratch_path = scratch.path();

    let checkout: PathBuf = if let Some(p) = from {
        ui::print_stage("Using", &format!("{} (--from)", p.display()));
        p.to_path_buf()
    } else {
        let dest = scratch_path.join("checkout");
        ui::print_stage("Cloning", &format!("{REPO_URL} → {}", dest.display()));
        git::clone(REPO_URL, &dest)?;
        ui::print_stage("Checking out", git_ref);
        git::checkout(&dest, git_ref)?;
        dest
    };

    let build = scratch_path.join("build");

    ui::print_stage("Configuring", &format!("cmake → {}", prefix.display()));
    let prefix_str = prefix.display().to_string();
    cmake::configure(
        &checkout,
        &build,
        &[
            ("CMAKE_INSTALL_PREFIX", prefix_str.as_str()),
            ("CMAKE_BUILD_TYPE", "Release"),
        ],
    )?;

    ui::print_stage("Building", "libbeethoven");
    cmake::build(&build, jobs)?;

    ui::print_stage("Installing", &format!("→ {}", prefix.display()));
    cmake::install(&build)?;

    let manifest_dst = env::manifest_path()?;
    cmake::copy_manifest(&build, &manifest_dst)?;

    persist_config(prefix, git_ref)?;

    ui::print_success(&format!(
        "libbeethoven installed to {}. Run `beethoven info` to verify.",
        prefix.display()
    ));
    // Scratch tempdir drops here.
    Ok(())
}

fn resolve_prefix(arg: Option<&Path>, cfg: &UserConfig) -> PathBuf {
    if let Some(p) = arg {
        return env::expand_tilde(p);
    }
    if let Some(p) = &cfg.prefix {
        return p.clone();
    }
    env::default_prefix()
}

fn validate_from_path(p: &Path) -> Result<()> {
    let marker = p.join("libbeethoven").join("CMakeLists.txt");
    if !marker.is_file() {
        return Err(CliError::config(format!(
            "--from path doesn't look like a Beethoven-Software clone: \
             missing {}",
            marker.display()
        )));
    }
    Ok(())
}

fn pre_flight(from: Option<&Path>) -> Result<()> {
    if from.is_none() {
        exec::require_tool("git", Some("install via your package manager"))?;
    }
    exec::require_tool(
        "cmake",
        Some("install via your package manager (cmake ≥ 3.20)"),
    )?;
    if which::which("g++").is_err() && which::which("clang++").is_err() {
        return Err(crate::error::CliError::missing_tool(
            "g++ or clang++".to_string(),
            Some("install build-essential / xcode-select / similar".into()),
        ));
    }
    Ok(())
}

fn persist_config(prefix: &Path, git_ref: &str) -> Result<()> {
    let mut cfg = UserConfig::load()?;
    cfg.prefix = Some(prefix.to_path_buf());
    cfg.git_ref = Some(git_ref.to_string());
    cfg.save()?;
    Ok(())
}
