//! `beethoven update` — uninstall the current install, then re-run
//! setup. No in-place fast-forward; we trust the cmake-install path
//! to handle whatever changed in the source tree.
//!
//! Note: `update` does *not* purge the user config or cache, because
//! it's about to write a fresh manifest. `commands::uninstall::run`
//! does purge — that's the user-facing "I'm done" flow.

use crate::cli::UpdateArgs;
use crate::commands::{setup, uninstall};
use crate::error::{CliError, Result};
use crate::state::UserConfig;

pub fn run(args: UpdateArgs) -> Result<()> {
    let cfg = UserConfig::load()?;
    let prefix = cfg.prefix.clone().ok_or_else(|| {
        CliError::config(
            "update needs `setup` to have been run first (no install prefix recorded)",
        )
    })?;
    // Same ref-resolution as `setup::run`: explicit --ref wins,
    // cfg.git_ref is a soft hint that gracefully falls back to HEAD
    // if it's stale, None means "use the remote's default branch."
    let git_ref = args.git_ref.or_else(|| cfg.git_ref.clone());

    // Step 1: tear down the current install. We do *not* purge the
    // user config / cache — setup will rewrite the manifest in a
    // moment, and we want the prefix preference preserved if the
    // user didn't pass --prefix.
    uninstall::do_uninstall(&prefix, /* purge_user_state = */ false)?;

    // Step 2: fresh clone + install.
    setup::do_setup(&prefix, git_ref.as_deref(), args.from.as_deref(), args.jobs)?;

    Ok(())
}
