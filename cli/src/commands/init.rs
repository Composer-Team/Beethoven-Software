//! `beethoven init` — scaffold a project in the current directory.
//!
//! Almost identical to `new`, except the destination is cwd and the
//! project name comes from the basename. Refuses if `Beethoven.toml`
//! is already present (so we never half-overwrite an existing project).

use crate::cli::{InitArgs, Platform};
use crate::error::{CliError, Result};
use crate::state::UserConfig;
use crate::template::{self, Vars};
use crate::ui;
use std::env;
use std::path::Path;
use std::process::Command;

pub fn run(args: InitArgs) -> Result<()> {
    let cwd = env::current_dir()
        .map_err(|e| CliError::config(format!("cannot read cwd: {e}")))?;

    let name = cwd
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| {
            CliError::config(format!(
                "cwd has no usable basename: {}",
                cwd.display()
            ))
        })?
        .to_string();
    validate_name(&name)?;

    if cwd.join("Beethoven.toml").exists() {
        return Err(CliError::config(
            "Beethoven.toml already exists in this directory".to_string(),
        ));
    }

    let cfg = UserConfig::load()?;
    let target = resolve_target(args.platform, &cfg);

    if !template::is_known_target(&target) {
        ui::print_warning(&format!(
            "target '{target}' is not in the CLI's known list — \
             the generated Beethoven.toml will not include a [platform.{target}] block."
        ));
    }

    let vars = Vars::new(&name, args.accel.as_deref(), &target);

    ui::print_stage("Initializing", &format!("{} ({})", cwd.display(), target));
    template::extract_to(&cwd, &vars)?;

    if args.vcs && !cwd.join(".git").exists() {
        ui::print_stage("Initializing", "git repo");
        git_init(&cwd)?;
        ui::print_note("run `git add . && git commit -m 'Initial commit'` to make the first commit");
    }

    ui::print_success(&format!(
        "project '{name}' initialized. Next: beethoven check"
    ));
    Ok(())
}

fn validate_name(name: &str) -> Result<()> {
    if name.is_empty() {
        return Err(CliError::usage("derived project name is empty"));
    }
    let first = name.chars().next().unwrap();
    if !first.is_ascii_alphabetic() {
        return Err(CliError::usage(format!(
            "directory name '{name}' must start with an ASCII letter to be a valid project name"
        )));
    }
    if !name
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
    {
        return Err(CliError::usage(format!(
            "directory name '{name}' may only contain ASCII letters, digits, '-', and '_'"
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
    "simulation".into()
}

fn git_init(dir: &Path) -> Result<()> {
    let mut cmd = Command::new("git");
    cmd.arg("init").arg(dir);
    crate::core::exec::run(&mut cmd)?;
    Ok(())
}
