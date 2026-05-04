//! `beethoven check` — validate Beethoven.toml + tool availability.
//!
//! Phase 3 scope: structural manifest validation + tool presence on
//! PATH. The chisel-elaboration step from `cli/docs/check.md` lands
//! in phase 5 alongside the build pipeline.
//!
//! Output is cargo-style: each check prints a single line with green
//! ✓ or red ✗, then a final summary. Failures are collected; we
//! don't short-circuit, so the user sees every problem at once.

use crate::error::{CliError, Result};
use crate::state::{target_to_platform, Project};
use crate::ui;
use console::style;

pub fn run() -> Result<()> {
    let project = Project::discover()?;
    let mut had_errors = false;

    // 1. Manifest schema
    match validate_manifest(&project) {
        Ok(()) => print_ok("Beethoven.toml"),
        Err(e) => {
            print_fail("Beethoven.toml", &e);
            had_errors = true;
        }
    }

    // 2. Tool availability
    match check_tools(&project) {
        Ok(found) => print_ok(&format!("tools: {}", found.join(" "))),
        Err(missing) => {
            for tool in &missing {
                print_fail(&format!("tool '{tool}'"), "not on PATH");
            }
            had_errors = true;
        }
    }

    println!();
    if had_errors {
        Err(CliError::config("one or more checks failed".to_string()))
    } else {
        ui::print_success("all checks passed");
        Ok(())
    }
}

fn validate_manifest(project: &Project) -> std::result::Result<(), String> {
    if project.manifest.project.name.trim().is_empty() {
        return Err("[project].name is empty".into());
    }

    let target = project
        .target()
        .ok_or_else(|| "[platform].target is required".to_string())?;

    if target_to_platform(target).is_none() {
        return Err(format!(
            "[platform].target '{target}': unrecognized (expected one of: \
             default, kria, kria2, aupzu3, aws-f1, aws-f2, u200, u250, u280, baremetal)"
        ));
    }

    // Note: there is no `[platform].build-mode` to validate — build
    // mode is per-invocation (set by the `beethoven` CLI command, or
    // passed as `--mode` to sbt). See Beethoven-Hardware's
    // Manifest.scala for rationale.

    // Per-target required keys.
    if matches!(target, "aupzu3" | "kria" | "kria2") {
        let spec = project.target_specific();
        let has_dram = spec
            .and_then(|t| t.get("dram-size-gb"))
            .is_some();
        if !has_dram {
            return Err(format!(
                "[platform.{target}] is missing required key 'dram-size-gb'"
            ));
        }
    }

    // beethoven-hardware: at least one of path / version must be
    // active. Fresh scaffolds leave both commented out so the user
    // makes a deliberate choice (pin a captured version vs. source-link
    // a sibling checkout). Without either, sbt has no beethoven-hardware
    // dep and the build won't elaborate — catching it here gives a
    // clearer message than sbt's "unresolved dependencies" error.
    let has_path = project.beethoven_hardware_path().is_some();
    let has_version = project.beethoven_hardware_version().is_some();
    if !has_path && !has_version {
        return Err(
            "[hardware.beethoven-hardware] has no active dependency — \
             uncomment 'path' or 'version' in Beethoven.toml"
                .into(),
        );
    }

    Ok(())
}

fn check_tools(_project: &Project) -> std::result::Result<Vec<String>, Vec<String>> {
    let required: Vec<&str> = vec!["cmake", "sbt"];

    let mut found = Vec::new();
    let mut missing = Vec::new();
    for tool in &required {
        if which::which(tool).is_ok() {
            found.push(tool.to_string());
        } else {
            missing.push(tool.to_string());
        }
    }

    if missing.is_empty() {
        Ok(found)
    } else {
        Err(missing)
    }
}

fn print_ok(msg: &str) {
    println!("  {} {}", style("✓").green().bold(), msg);
}

fn print_fail(what: &str, reason: &str) {
    println!(
        "  {} {what}: {}",
        style("✗").red().bold(),
        style(reason).red()
    );
}
