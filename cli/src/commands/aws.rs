//! `beethoven aws ...` — AWS F2 helper commands.
//!
//! Keep long-running remote work explicit and outside this CLI for now:
//! `upload` only synchronizes the locally generated CL package.

use crate::cli::{AwsCommand, AwsUploadArgs};
use crate::core::exec;
use crate::error::{CliError, Result};
use crate::state::Project;
use crate::ui;
use std::path::{Path, PathBuf};
use std::process::Command;

pub fn run(command: AwsCommand) -> Result<()> {
    match command {
        AwsCommand::Upload(args) => upload(args),
    }
}

fn upload(args: AwsUploadArgs) -> Result<()> {
    let project = Project::discover()?;
    require_aws_f2(&project)?;
    exec::require_tool("rsync", Some("install rsync and ensure it is on PATH"))?;
    exec::require_tool(
        "ssh",
        Some("install OpenSSH client and ensure ssh is on PATH"),
    )?;

    let package_dir = aws_f2_package_dir(&project);
    if !package_dir.is_dir() {
        return Err(CliError::config(format!(
            "missing AWS F2 package at {}.\n\
             Run `beethoven build hw --release` first.",
            package_dir.display()
        )));
    }
    require_file(&package_dir.join("build_beethoven_f2.sh"))?;
    require_dir(&package_dir.join("design"))?;
    require_dir(&package_dir.join("build").join("scripts"))?;
    require_dir(&package_dir.join("build").join("constraints"))?;

    let mut cmd = Command::new("rsync");
    cmd.arg("-avz");
    if args.delete {
        cmd.arg("--delete");
    }
    if let Some(key) = args.key {
        cmd.arg("-e")
            .arg(format!("ssh -i {}", shell_quote_path(&key)));
    }
    cmd.arg(format!("{}/", package_dir.display())).arg(format!(
        "{}:{}/",
        args.host,
        args.remote_dir.trim_end_matches('/')
    ));

    ui::print_stage("Uploading", "AWS F2 CL package");
    exec::run(&mut cmd)?;
    ui::print_success(&format!(
        "uploaded {} to {}:{}/",
        package_dir.display(),
        args.host,
        args.remote_dir.trim_end_matches('/')
    ));
    Ok(())
}

fn require_aws_f2(project: &Project) -> Result<()> {
    match project.target() {
        Some("aws-f2") => Ok(()),
        Some(other) => Err(CliError::config(format!(
            "`beethoven aws upload` requires [platform].target = \"aws-f2\"; found \"{other}\""
        ))),
        None => Err(CliError::config(
            "`beethoven aws upload` requires [platform].target = \"aws-f2\"".to_string(),
        )),
    }
}

fn aws_f2_package_dir(project: &Project) -> PathBuf {
    project
        .root
        .join("target")
        .join("synthesis")
        .join("aws")
        .join("cl_beethoven_top")
}

fn require_dir(path: &Path) -> Result<()> {
    if path.is_dir() {
        Ok(())
    } else {
        Err(CliError::config(format!(
            "incomplete AWS F2 package: missing directory {}",
            path.display()
        )))
    }
}

fn require_file(path: &Path) -> Result<()> {
    if path.is_file() {
        Ok(())
    } else {
        Err(CliError::config(format!(
            "incomplete AWS F2 package: missing file {}",
            path.display()
        )))
    }
}

fn shell_quote_path(path: &Path) -> String {
    let s = path.display().to_string();
    if s.is_empty() {
        return "''".to_string();
    }
    let needs_quote = s
        .chars()
        .any(|c| !(c.is_ascii_alphanumeric() || "._-/=:@,+~".contains(c)));
    if needs_quote {
        format!("'{}'", s.replace('\'', "'\\''"))
    } else {
        s
    }
}
