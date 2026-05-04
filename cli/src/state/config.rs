//! User-level config at `~/.config/beethoven/config.toml` (XDG-aware).
//! Written by `setup`, consulted by every subcommand that needs a
//! libbeethoven prefix or related defaults.
//!
//! Schema documented in `cli/docs/config.md`.

use crate::error::{CliError, Result};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct UserConfig {
    /// Where libbeethoven is installed (e.g. `~/.local`).
    pub prefix: Option<PathBuf>,

    /// Tracked git ref (default "main"). Used by `update` to know
    /// which ref to re-clone from upstream.
    #[serde(rename = "ref")]
    pub git_ref: Option<String>,

    /// Default platform for `new` / `init` when --platform is omitted.
    pub default_platform: Option<String>,

    /// Beethoven-Hardware coordinates captured by `setup` after a
    /// successful `sbt publishLocal`. When present, `init` / `new`
    /// scaffold a project that depends on the version published in
    /// the local Ivy cache (`~/.ivy2/local/`); when absent, they fall
    /// back to a sibling-path source link (the legacy default for
    /// framework devs). All three move together — set or unset as a
    /// group via setup.
    pub hardware_version: Option<String>,
    pub hardware_organization: Option<String>,
    pub hardware_artifact: Option<String>,
}

impl UserConfig {
    /// Resolve `~/.config/beethoven/config.toml` (XDG-aware).
    pub fn path() -> Result<PathBuf> {
        let dir = dirs::config_dir().ok_or_else(|| {
            CliError::config("cannot resolve user config dir; set $XDG_CONFIG_HOME or $HOME")
        })?;
        Ok(dir.join("beethoven").join("config.toml"))
    }

    /// Load the user config. Returns the default (all `None`) if the
    /// file doesn't exist — first-run users haven't done `setup` yet.
    pub fn load() -> Result<Self> {
        let path = Self::path()?;
        match fs::read_to_string(&path) {
            Ok(s) => toml::from_str(&s).map_err(|e| {
                CliError::config(format!("malformed user config at {}: {e}", path.display()))
            }),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(Self::default()),
            Err(e) => Err(CliError::config(format!(
                "cannot read user config at {}: {e}",
                path.display()
            ))),
        }
    }

    /// Persist the user config. Creates the parent directory if needed.
    pub fn save(&self) -> Result<()> {
        let path = Self::path()?;
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|e| {
                CliError::config(format!(
                    "cannot create config dir {}: {e}",
                    parent.display()
                ))
            })?;
        }
        let body = toml::to_string_pretty(self)
            .map_err(|e| anyhow::anyhow!("serialize user config: {e}"))?;
        fs::write(&path, body).map_err(|e| {
            CliError::config(format!(
                "cannot write user config to {}: {e}",
                path.display()
            ))
        })?;
        Ok(())
    }
}
