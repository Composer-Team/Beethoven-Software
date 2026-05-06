//! `beethoven new <name>` — scaffold a project in a new directory.
//!
//! Workflow:
//!   1. Validate name (ASCII identifier-ish).
//!   2. Resolve `accel`, `target`. Warn if target isn't recognized
//!      (the generated Beethoven.toml will be missing platform-
//!      specific defaults).
//!   3. Compute destination = cwd/<name>; refuse if it exists.
//!   4. Extract the embedded template, applying placeholder
//!      substitution.
//!   5. Optionally `git init` if `--vcs`.

use crate::cli::{NewArgs, Platform};
use crate::error::{CliError, Result};
use crate::state::UserConfig;
use crate::template::{self, Flavor, HardwareCoords, Vars};
use crate::ui;
use std::env;
use std::path::PathBuf;
use std::process::Command;

pub fn run(args: NewArgs) -> Result<()> {
    validate_name(&args.name)?;
    let cfg = UserConfig::load()?;
    let target = resolve_target(args.platform, &cfg);

    if !template::is_known_target(&target) {
        ui::print_warning(&format!(
            "target '{target}' is not in the CLI's known list — \
             the generated Beethoven.toml will not include a [platform.{target}] block. \
             You may need to add platform-specific config by hand."
        ));
    }

    let dest = current_dir()?.join(&args.name);
    if dest.exists() {
        return Err(CliError::config(format!(
            "destination already exists: {}",
            dest.display()
        )));
    }

    let hw = hardware_coords_from_cfg(&cfg);
    if hw.is_none() {
        ui::print_note(
            "no Beethoven-Hardware version captured in user config; scaffolding \
             with the legacy `path = \"../Beethoven-Hardware\"` source link. \
             Run `beethoven setup` to switch new projects to a published version.",
        );
    }
    let flavor = if args.verilog { Flavor::Verilog } else { Flavor::Chisel };
    let vars = Vars::new(&args.name, args.accel.as_deref(), &target, hw.as_ref(), flavor);

    let flavor_label = if args.verilog { "verilog" } else { "chisel" };
    ui::print_stage(
        "Creating",
        &format!("{} ({}, {})", dest.display(), target, flavor_label),
    );
    template::extract_to(&dest, &vars, flavor)?;

    if args.vcs {
        ui::print_stage("Initializing", "git repo");
        git_init(&dest)?;
        ui::print_note("run `git add . && git commit -m 'Initial commit'` to make the first commit");
    }

    ui::print_success(&format!(
        "project '{}' created. Next: cd {} && beethoven check",
        args.name, args.name
    ));
    Ok(())
}

/// Pull captured Beethoven-Hardware coordinates from user config.
/// Returns None when any of the trio (org, name, version) is missing,
/// which signals "fall back to path mode" to `Vars::new`.
fn hardware_coords_from_cfg(cfg: &UserConfig) -> Option<HardwareCoords> {
    Some(HardwareCoords {
        organization: cfg.hardware_organization.clone()?,
        artifact: cfg.hardware_artifact.clone()?,
        version: cfg.hardware_version.clone()?,
    })
}

/// Reject names that would break scala identifiers, paths, or be
/// confusing on the filesystem. The pattern matches what cargo's
/// `cargo new` accepts plus our own snake/PascalCase derivation.
fn validate_name(name: &str) -> Result<()> {
    if name.is_empty() {
        return Err(CliError::usage("project name is empty"));
    }
    if name == "." || name == ".." {
        return Err(CliError::usage(format!("invalid project name '{name}'")));
    }
    if name.contains('/') || name.contains('\\') {
        return Err(CliError::usage(format!(
            "project name '{name}' contains a path separator"
        )));
    }
    let first = name.chars().next().unwrap();
    if !first.is_ascii_alphabetic() {
        return Err(CliError::usage(format!(
            "project name '{name}' must start with an ASCII letter"
        )));
    }
    if !name
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
    {
        return Err(CliError::usage(format!(
            "project name '{name}' may only contain ASCII letters, digits, '-', and '_'"
        )));
    }
    Ok(())
}

fn resolve_target(platform: Option<Platform>, cfg: &UserConfig) -> String {
    if let Some(p) = platform {
        return p.target_name().to_string();
    }
    if let Some(p) = &cfg.default_platform {
        return p.clone();
    }
    "default".into()
}

fn current_dir() -> Result<PathBuf> {
    env::current_dir()
        .map_err(|e| CliError::config(format!("cannot read cwd: {e}")))
}

fn git_init(dir: &std::path::Path) -> Result<()> {
    let mut cmd = Command::new("git");
    cmd.arg("init").arg(dir);
    crate::core::exec::run(&mut cmd)?;
    Ok(())
}
