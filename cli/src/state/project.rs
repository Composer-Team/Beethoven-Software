//! Per-project Beethoven.toml manifest.
//!
//! Schema (per `Beethoven-Software/docs/beethoven-toml-reference.md`):
//!
//!   [project]
//!   name = "..."
//!   version = "0.0.1"            # optional
//!
//!   [hardware]                   # entire section optional
//!   src-dir = "hw"               # optional
//!
//!   [hardware.beethoven-hardware]
//!   path = "..."                 # one of path / version
//!   version = "..."
//!
//!   [software]                   # optional
//!   src-dir = "sw"               # optional
//!
//!   [platform]
//!   target = "..."               # required: default | kria | aupzu3 | ...
//!   build-mode = "..."           # required: simulation | synthesis
//!
//!   [platform.<target>]          # required for some targets (e.g. aupzu3)
//!   ...
//!
//!   [build]                      # optional
//!   output-dir = "target"        # optional
//!
//! The CLI parses `project` strictly (it depends on `name`) and the
//! rest permissively as `toml::value::Table`s, with helper accessors
//! that pull out the keys we care about. New schema additions don't
//! force code changes for every field.

use crate::error::{CliError, Result};
use serde::Deserialize;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

const MANIFEST_NAME: &str = "Beethoven.toml";

#[derive(Debug, Clone, Deserialize)]
pub struct Manifest {
    pub project: ProjectSection,

    #[serde(default)]
    pub hardware: toml::value::Table,

    #[serde(default)]
    pub software: toml::value::Table,

    #[serde(default)]
    pub platform: toml::value::Table,

    #[serde(default)]
    pub build: toml::value::Table,
}

#[derive(Debug, Clone, Deserialize)]
pub struct ProjectSection {
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct Project {
    pub root: PathBuf,
    pub manifest: Manifest,
}

/// Resolved hardware platform — the runtime's view, after the user's
/// target name (e.g. `kria`, `aws-f1`) is mapped to one of three
/// physical buckets.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Platform {
    Discrete,
    Zynq,
    Baremetal,
}

impl Platform {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Discrete => "discrete",
            Self::Zynq => "zynq",
            Self::Baremetal => "baremetal",
        }
    }
}

/// Map a target name from `[platform].target` to a resolved platform.
/// Returns `None` for unrecognized names.
///
/// `"default"` is the generic sim-tuned target (was named `"simulation"`
/// pre-rename — disambiguated against the build-mode `simulation`).
pub fn target_to_platform(target: &str) -> Option<Platform> {
    match target {
        "default" => Some(Platform::Discrete),
        t if t.starts_with("aws-") => Some(Platform::Discrete),
        "u200" | "u250" | "u280" => Some(Platform::Discrete),
        "kria" | "kria2" | "aupzu3" => Some(Platform::Zynq),
        "baremetal" => Some(Platform::Baremetal),
        _ => None,
    }
}

/// True if the target has a synthesis flow. The generic `default`
/// target and `baremetal` don't — anything else does.
pub fn target_supports_synth(target: &str) -> bool {
    !matches!(target, "default" | "baremetal")
}

impl Project {
    /// Discover the current project. Honors `BEETHOVEN_PROJECT_ROOT`
    /// if set; otherwise walks up from cwd.
    pub fn discover() -> Result<Self> {
        if let Ok(env_root) = env::var("BEETHOVEN_PROJECT_ROOT") {
            return Self::load(Path::new(&env_root));
        }
        let cwd = env::current_dir()
            .map_err(|e| CliError::config(format!("cannot read cwd: {e}")))?;
        Self::discover_from(&cwd)
    }

    /// Walk up from `start` looking for `Beethoven.toml`.
    pub fn discover_from(start: &Path) -> Result<Self> {
        let mut dir = start.to_path_buf();
        loop {
            if dir.join(MANIFEST_NAME).is_file() {
                return Self::load(&dir);
            }
            if !dir.pop() {
                return Err(CliError::config(format!(
                    "not in a Beethoven project (no {MANIFEST_NAME} found from {})",
                    start.display()
                )));
            }
        }
    }

    /// Load the manifest from a known root.
    pub fn load(root: &Path) -> Result<Self> {
        let manifest_path = root.join(MANIFEST_NAME);
        let body = fs::read_to_string(&manifest_path).map_err(|e| {
            CliError::config(format!("cannot read {}: {e}", manifest_path.display()))
        })?;
        let manifest: Manifest = toml::from_str(&body).map_err(|e| {
            CliError::config(format!("malformed {}: {e}", manifest_path.display()))
        })?;
        Ok(Self {
            root: root.to_path_buf(),
            manifest,
        })
    }

    // ---- platform accessors ----

    /// `[platform].target`, if set.
    pub fn target(&self) -> Option<&str> {
        self.manifest.platform.get("target")?.as_str()
    }

    /// Resolved `Platform` for the project's target. None if the
    /// target is unset or unrecognized.
    pub fn platform(&self) -> Option<Platform> {
        target_to_platform(self.target()?)
    }

    /// `[platform.<target>]` table, if present.
    pub fn target_specific(&self) -> Option<&toml::value::Table> {
        let target = self.target()?;
        self.manifest.platform.get(target)?.as_table()
    }

    /// `[platform].simulator`, defaulting to `"icarus"` for sim
    /// builds when not set. Read by the runtime build to pick which
    /// simulator backend cmake compiles in.
    ///
    /// Why icarus instead of verilator: verilator is faster, but
    /// `runtime/include/core/data_channel.h` has an open issue with
    /// the `VlWide<N>` types Verilator emits for SimulationPlatform's
    /// wide buses. Icarus avoids that entire codepath. Switch back
    /// once the widebus issue is fixed
    /// (`docs/issues/verilator-widebus.md`).
    pub fn simulator(&self) -> &str {
        self.manifest
            .platform
            .get("simulator")
            .and_then(|v| v.as_str())
            .unwrap_or("icarus")
    }

    // ---- hardware accessors ----

    /// `[hardware.beethoven-hardware].path`, if set.
    pub fn beethoven_hardware_path(&self) -> Option<&str> {
        self.manifest
            .hardware
            .get("beethoven-hardware")?
            .as_table()?
            .get("path")?
            .as_str()
    }

    /// `[hardware.beethoven-hardware].version`, if set.
    pub fn beethoven_hardware_version(&self) -> Option<&str> {
        self.manifest
            .hardware
            .get("beethoven-hardware")?
            .as_table()?
            .get("version")?
            .as_str()
    }
}
