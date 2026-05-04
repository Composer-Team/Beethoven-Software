//! `beethoven uninstall` — remove libbeethoven from the install
//! prefix, then clear the CLI's cache and user config so the system
//! is back to first-run state.
//!
//! This is intentionally aggressive: there is no `--keep-config` /
//! `--keep-cache` flag for v1. `update` calls a private helper that
//! skips the user-state purge, but the user-facing command always
//! purges.

use crate::cli::UninstallArgs;
use crate::core::env;
use crate::error::Result;
use crate::state::UserConfig;
use crate::tools::cmake;
use crate::ui;
use dialoguer::Confirm;
use std::path::Path;

pub fn run(args: UninstallArgs) -> Result<()> {
    let cfg = UserConfig::load()?;
    let prefix = match &cfg.prefix {
        Some(p) => p.clone(),
        None => {
            ui::print_note("nothing to uninstall — `setup` has not been run.");
            return Ok(());
        }
    };

    if !args.yes {
        let prompt = format!(
            "Remove libbeethoven from {} and clear CLI cache + config?",
            prefix.display()
        );
        let proceed = Confirm::new()
            .with_prompt(prompt)
            .default(false)
            .interact()
            .map_err(|e| anyhow::anyhow!("could not read confirmation prompt: {e}"))?;
        if !proceed {
            ui::print_note("aborted.");
            return Ok(());
        }
    }

    do_uninstall(&prefix, /* purge_user_state = */ true)?;
    ui::print_success("uninstalled.");
    Ok(())
}

/// Reusable worker. With `purge_user_state = false`, leaves the user
/// config and cache alone — used by `update`, which is about to
/// rewrite them.
pub fn do_uninstall(prefix: &Path, purge_user_state: bool) -> Result<()> {
    ui::print_stage(
        "Removing",
        &format!("libbeethoven from {}", prefix.display()),
    );

    let manifest = env::manifest_path()?;
    if manifest.is_file() {
        cmake::uninstall_from_manifest(&manifest, prefix)?;
        let _ = std::fs::remove_file(&manifest);
    }

    cmake::remove_registry_entries_for(prefix)?;

    if purge_user_state {
        let cache = env::cache_dir();
        if cache.is_dir() {
            ui::print_stage("Removing", &format!("CLI cache: {}", cache.display()));
            let _ = std::fs::remove_dir_all(&cache);
        }
        if let Ok(config_path) = UserConfig::path() {
            if let Some(config_dir) = config_path.parent() {
                if config_dir.is_dir() {
                    ui::print_stage(
                        "Removing",
                        &format!("user config: {}", config_dir.display()),
                    );
                    let _ = std::fs::remove_dir_all(config_dir);
                }
            }
        }
    }

    Ok(())
}
