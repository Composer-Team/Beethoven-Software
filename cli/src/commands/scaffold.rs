//! Shared project scaffolding for `beethoven new` and `beethoven init`.
//!
//! The wrappers decide where the project lives. This module owns the
//! common behavior so both commands render templates, resolve config,
//! and initialize optional VCS state the same way.

use crate::cli::Platform;
use crate::error::{CliError, Result};
use crate::state::UserConfig;
use crate::template::{self, Flavor, HardwareCoords, Vars};
use crate::ui;
use std::path::{Path, PathBuf};
use std::process::Command;

pub struct ScaffoldRequest {
    pub name: String,
    pub dest: PathBuf,
    pub platform: Option<Platform>,
    pub accel: Option<String>,
    pub verilog: bool,
    pub vcs: bool,
    pub command: ScaffoldCommand,
}

#[derive(Clone, Copy)]
pub enum ScaffoldCommand {
    New,
    Init,
}

pub fn run(req: ScaffoldRequest) -> Result<()> {
    validate_project_name(&req.name)?;

    let cfg = UserConfig::load()?;
    let target = resolve_target(req.platform, &cfg);

    warn_if_unknown_target(&target);

    let hw = hardware_coords_from_cfg(&cfg);
    if hw.is_none() {
        ui::print_note(
            "no Beethoven-Hardware version captured in user config; scaffolding \
             with the legacy `path = \"../Beethoven-Hardware\"` source link. \
             Run `beethoven setup` to switch new projects to a published version.",
        );
    }

    let flavor = if req.verilog {
        Flavor::Verilog
    } else {
        Flavor::Chisel
    };
    let vars = Vars::new(
        &req.name,
        req.accel.as_deref(),
        &target,
        hw.as_ref(),
        flavor,
    );

    ui::print_stage(
        req.command.stage_label(),
        &format!(
            "{} ({}, {})",
            req.dest.display(),
            target,
            flavor_label(flavor)
        ),
    );
    template::extract_to(&req.dest, &vars, flavor)?;

    if req.vcs && should_init_git(req.command, &req.dest) {
        ui::print_stage("Initializing", "git repo");
        git_init(&req.dest)?;
        ui::print_note(
            "run `git add . && git commit -m 'Initial commit'` to make the first commit",
        );
    }

    ui::print_success(&req.command.success_message(&req.name));
    Ok(())
}

pub fn validate_project_name(name: &str) -> Result<()> {
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

fn warn_if_unknown_target(target: &str) {
    if !template::is_known_target(target) {
        ui::print_warning(&format!(
            "target '{target}' is not in the CLI's known list — \
             the generated Beethoven.toml will not include a [platform.{target}] block. \
             You may need to add platform-specific config by hand."
        ));
    }
}

fn hardware_coords_from_cfg(cfg: &UserConfig) -> Option<HardwareCoords> {
    Some(HardwareCoords {
        organization: cfg.hardware_organization.clone()?,
        artifact: cfg.hardware_artifact.clone()?,
        version: cfg.hardware_version.clone()?,
    })
}

fn flavor_label(flavor: Flavor) -> &'static str {
    match flavor {
        Flavor::Chisel => "chisel",
        Flavor::Verilog => "verilog",
    }
}

fn should_init_git(command: ScaffoldCommand, dest: &Path) -> bool {
    match command {
        ScaffoldCommand::New => true,
        ScaffoldCommand::Init => !dest.join(".git").exists(),
    }
}

fn git_init(dir: &Path) -> Result<()> {
    let mut cmd = Command::new("git");
    cmd.arg("init").arg(dir);
    crate::core::exec::run(&mut cmd)?;
    Ok(())
}

impl ScaffoldCommand {
    fn stage_label(self) -> &'static str {
        match self {
            Self::New => "Creating",
            Self::Init => "Initializing",
        }
    }

    fn success_message(self, name: &str) -> String {
        match self {
            Self::New => format!("project '{name}' created. Next: cd {name} && beethoven check"),
            Self::Init => format!("project '{name}' initialized. Next: beethoven check"),
        }
    }
}
