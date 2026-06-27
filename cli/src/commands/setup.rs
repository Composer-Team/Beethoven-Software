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
use crate::tools::{cmake, git, sbt};
use crate::ui;
use std::path::{Path, PathBuf};

const REPO_URL: &str = "https://github.com/Composer-Team/Beethoven-Software";
const HARDWARE_REPO_URL: &str = "https://github.com/Composer-Team/Beethoven-Hardware";

pub fn run(args: SetupArgs) -> Result<()> {
    let cfg = UserConfig::load()?;
    let prefix = resolve_prefix(args.prefix.as_deref(), &cfg);
    // Ref resolution: explicit --ref always wins; cfg.git_ref is a
    // soft hint (it gets soft-fallback to HEAD if it's stale); when
    // neither is set we let `git clone` land on the remote's default
    // branch and persist whatever that turns out to be. The old
    // hardcoded "main" default bit users when the remote was actually
    // on `master`.
    let git_ref = args.git_ref.or_else(|| cfg.git_ref.clone());
    let hardware_ref = args.hardware_ref;

    prompt_brew_install_if_macos(!args.no_hardware)?;

    do_setup(&prefix, git_ref.as_deref(), args.from.as_deref(), args.jobs)?;

    if !args.no_hardware {
        do_setup_hardware(args.hardware_from.as_deref(), hardware_ref.as_deref())?;
    }
    Ok(())
}

/// On macOS, offer to `brew install` build-time tools that the rest
/// of the workflow needs. `sbt` is required by the hardware step;
/// `iverilog` isn't needed by `setup` itself but is the default
/// simulator picked up by `beethoven build` later, so checking it
/// here saves the user a fail-then-install round trip. No-op when
/// none are missing, when brew isn't installed (we just print a
/// note), or when the user declines — the existing per-stage
/// `require_tool` calls still gate hard failures.
fn prompt_brew_install_if_macos(want_sbt: bool) -> Result<()> {
    if !cfg!(target_os = "macos") {
        return Ok(());
    }

    let mut candidates: Vec<(&str, &str)> = Vec::new();
    if want_sbt {
        candidates.push(("sbt", "sbt"));
    }
    candidates.push(("iverilog", "icarus-verilog"));

    let missing: Vec<(&str, &str)> = candidates
        .into_iter()
        .filter(|(bin, _)| which::which(bin).is_err())
        .collect();
    if missing.is_empty() {
        return Ok(());
    }

    let bins: Vec<&str> = missing.iter().map(|(b, _)| *b).collect();
    let formulas: Vec<&str> = missing.iter().map(|(_, f)| *f).collect();

    if which::which("brew").is_err() {
        ui::print_warning(&format!(
            "missing tools needed by later steps: {}. Install Homebrew \
             (https://brew.sh) and run `brew install {}`, or install \
             them manually.",
            bins.join(", "),
            formulas.join(" "),
        ));
        return Ok(());
    }

    let prompt = format!(
        "Missing: {}. Install via `brew install {}`?",
        bins.join(", "),
        formulas.join(" "),
    );
    let proceed = dialoguer::Confirm::new()
        .with_prompt(prompt)
        .default(true)
        .interact()
        .map_err(|e| anyhow::anyhow!("could not read confirmation prompt: {e}"))?;
    if !proceed {
        return Ok(());
    }

    ui::print_stage(
        "Installing",
        &format!("brew install {}", formulas.join(" ")),
    );
    let mut cmd = std::process::Command::new("brew");
    cmd.arg("install").args(&formulas);
    exec::run(&mut cmd)?;
    Ok(())
}

/// The reusable worker. Called by `setup::run` directly and by
/// `update::run` after it runs the uninstall step.
///
/// `git_ref = None` means "use whatever branch the clone lands on"
/// (the remote's default HEAD). When the user passes a ref but it
/// doesn't exist in the clone — e.g. a stale `ref = "main"` from a
/// previous CLI version that hardcoded that default — we warn and
/// stay on HEAD instead of failing. The persisted ref is always
/// whatever HEAD is at the end, so `info` and the next `update` see
/// the truth.
pub fn do_setup(
    prefix: &Path,
    git_ref: Option<&str>,
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
        if let Some(r) = git_ref {
            if git::ref_exists(&dest, r) {
                ui::print_stage("Checking out", r);
                git::checkout(&dest, r)?;
            } else {
                ui::print_warning(&format!(
                    "ref '{r}' not found in clone; staying on the repo's default branch"
                ));
            }
        }
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

    // Persist whatever branch we ended up on. With no --ref this
    // captures the remote's default (e.g. "master"), so a future
    // `update` reads back the right thing and doesn't fall over on
    // the stale-"main" trap that motivated this refactor. With
    // `--from` (a user-managed checkout) we still record current
    // HEAD for `info` to display.
    let resolved_ref =
        git::current_branch(&checkout).unwrap_or_else(|_| git_ref.unwrap_or("master").to_string());
    persist_config(prefix, &resolved_ref)?;

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
        Some("install via your package manager (cmake ≥ 3.16)"),
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

// ---- hardware install (sbt publishLocal) ----

/// Clone (or git pull) Beethoven-Hardware into the CLI-managed
/// directory and run `sbt publishLocal` so its jar lands in
/// `~/.ivy2/local/`. Captures the published coordinates into
/// UserConfig so `init` / `new` can scaffold projects that resolve
/// against this exact version.
///
/// `from` is an escape hatch for framework devs: when provided, we
/// publishLocal the user's existing checkout in place and skip the
/// CLI-managed clone. The path is otherwise untouched.
pub fn do_setup_hardware(from: Option<&Path>, git_ref: Option<&str>) -> Result<()> {
    exec::require_tool("sbt", Some("install via your package manager (sbt 1.x)"))?;
    if from.is_none() {
        exec::require_tool("git", Some("install via your package manager"))?;
    }

    let src = if let Some(p) = from {
        validate_hardware_path(p)?;
        ui::print_stage("Using", &format!("{} (--hardware-from)", p.display()));
        p.to_path_buf()
    } else {
        ensure_hardware_clone(git_ref)?
    };

    let coords = parse_hardware_coords(&src)?;
    ui::print_stage(
        "Publishing",
        &format!(
            "{}:{} {} → ~/.ivy2/local",
            coords.organization, coords.artifact, coords.version
        ),
    );
    sbt::publish_local(&src)?;

    persist_hardware_config(&coords)?;
    ui::print_success(&format!(
        "Beethoven-Hardware {} published locally; new projects will resolve it via `version`.",
        coords.version
    ));
    Ok(())
}

fn validate_hardware_path(p: &Path) -> Result<()> {
    let marker = p.join("build.sbt");
    if !marker.is_file() {
        return Err(CliError::config(format!(
            "--hardware-from path doesn't look like a Beethoven-Hardware clone: \
             missing {}",
            marker.display()
        )));
    }
    Ok(())
}

/// Resolve the CLI-managed clone, cloning or fetch+checkout as
/// needed. With `git_ref = None` we leave the clone on whatever HEAD
/// the remote chose (e.g. "master") — same self-healing strategy as
/// the SW path. With Some(r), we checkout if it exists, warn-and-
/// stay-on-HEAD if it doesn't.
fn ensure_hardware_clone(git_ref: Option<&str>) -> Result<PathBuf> {
    let dest = env::hardware_src_dir();
    if dest.join(".git").is_dir() {
        ui::print_stage("Updating", &format!("{} (git fetch)", dest.display()));
        git::fetch_all(&dest)?;
        if let Some(r) = git_ref {
            checkout_or_warn(&dest, r)?;
        }
    } else {
        if dest.exists() {
            return Err(CliError::config(format!(
                "{} exists but isn't a git checkout; remove it and re-run setup, \
                 or pass --hardware-from <path> to use it as-is.",
                dest.display()
            )));
        }
        if let Some(parent) = dest.parent() {
            std::fs::create_dir_all(parent).map_err(|e| {
                CliError::config(format!("cannot create {}: {e}", parent.display()))
            })?;
        }
        ui::print_stage(
            "Cloning",
            &format!("{HARDWARE_REPO_URL} → {}", dest.display()),
        );
        git::clone(HARDWARE_REPO_URL, &dest)?;
        if let Some(r) = git_ref {
            checkout_or_warn(&dest, r)?;
        }
    }
    Ok(dest)
}

/// Soft checkout: if the ref exists, switch to it; otherwise warn and
/// leave the clone on whatever HEAD it's at. Mirrors the same logic
/// used in `do_setup` for the software path.
fn checkout_or_warn(repo: &Path, git_ref: &str) -> Result<()> {
    if git::ref_exists(repo, git_ref) {
        ui::print_stage("Checking out", git_ref);
        git::checkout(repo, git_ref)?;
    } else {
        ui::print_warning(&format!(
            "ref '{git_ref}' not found in clone; staying on the repo's default branch"
        ));
    }
    Ok(())
}

#[derive(Debug)]
struct HardwareCoords {
    organization: String,
    artifact: String,
    version: String,
}

/// Parse `organization`, `name`, and `version` out of the cloned
/// Beethoven-Hardware `build.sbt`. We deliberately use a simple
/// regex-style line scan rather than an sbt evaluation: this runs
/// before `sbt publishLocal`, so we can't shell out to sbt for
/// metadata without doubling the runtime.
fn parse_hardware_coords(src: &Path) -> Result<HardwareCoords> {
    let path = src.join("build.sbt");
    let body = std::fs::read_to_string(&path)
        .map_err(|e| CliError::config(format!("cannot read {}: {e}", path.display())))?;

    let extract = |key: &str| -> Option<String> {
        for line in body.lines() {
            let trimmed = line.trim_start();
            if trimmed.starts_with("//") {
                continue;
            }
            if let Some(after_key) = trimmed.strip_prefix(key) {
                let after = after_key.trim_start();
                if !after.starts_with(":=") {
                    continue;
                }
                let rest = &after[2..];
                if let Some(start) = rest.find('"') {
                    if let Some(end_rel) = rest[start + 1..].find('"') {
                        return Some(rest[start + 1..start + 1 + end_rel].to_string());
                    }
                }
            }
        }
        None
    };

    let organization = extract("organization").ok_or_else(|| {
        CliError::config(format!(
            "could not parse `organization := \"...\"` from {}",
            path.display()
        ))
    })?;
    let artifact = extract("name").ok_or_else(|| {
        CliError::config(format!(
            "could not parse `name := \"...\"` from {}",
            path.display()
        ))
    })?;
    let version = extract("version").ok_or_else(|| {
        CliError::config(format!(
            "could not parse `version := \"...\"` from {}",
            path.display()
        ))
    })?;

    Ok(HardwareCoords {
        organization,
        artifact,
        version,
    })
}

fn persist_hardware_config(coords: &HardwareCoords) -> Result<()> {
    let mut cfg = UserConfig::load()?;
    cfg.hardware_organization = Some(coords.organization.clone());
    cfg.hardware_artifact = Some(coords.artifact.clone());
    cfg.hardware_version = Some(coords.version.clone());
    cfg.save()?;
    Ok(())
}
