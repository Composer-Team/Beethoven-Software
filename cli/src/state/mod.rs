//! Loaded user / project state.
//!
//! `UserConfig` is the per-machine config (`~/.config/beethoven/config.toml`).
//! `Project` is the per-project manifest (`Beethoven.toml`) plus its root.
//!
//! Re-exports the most-touched types at the module level so callers
//! can write `use crate::state::Project;` without spelling out the
//! inner submodule.

pub mod config;
pub mod project;

pub use config::UserConfig;
pub use project::{target_supports_synth, target_to_platform, Manifest, Platform, Project};
