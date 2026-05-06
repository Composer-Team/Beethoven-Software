//! Embedded template tree + extraction.
//!
//! The template at `cli/template/` is baked into the binary at compile
//! time via `include_dir!`. At runtime, `extract_to` walks the
//! embedded tree, applies `{{var}}` substitution to filenames and
//! `.tmpl` file contents, and writes everything to the destination
//! directory. Files without `.tmpl` are copied verbatim.
//!
//! Substitution variables — see `Vars`.

use crate::error::{CliError, Result};
use include_dir::{include_dir, Dir, DirEntry};
use std::fs;
use std::path::Path;

/// Embedded template trees. Paths are resolved at compile time
/// against `$CARGO_MANIFEST_DIR`. There's one tree per project flavor;
/// `Flavor::tree()` picks the right one at runtime.
const TEMPLATE_CHISEL: Dir<'_> = include_dir!("$CARGO_MANIFEST_DIR/template");
const TEMPLATE_VERILOG: Dir<'_> = include_dir!("$CARGO_MANIFEST_DIR/template-verilog");

/// Which project skeleton to scaffold. Selects both the embedded
/// template tree and the substitution rule for `{{system}}`:
///   - Chisel:  system = "my" + accel  (e.g. myVectorAdd)
///   - Verilog: system = accel + "Core" (e.g. VectorAddCore), matching
///     the convention in verilog-example/Kreyvium and SHAKE256.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Flavor {
    Chisel,
    Verilog,
}

impl Flavor {
    fn tree(self) -> &'static Dir<'static> {
        match self {
            Self::Chisel => &TEMPLATE_CHISEL,
            Self::Verilog => &TEMPLATE_VERILOG,
        }
    }

    fn system_name(self, accel: &str) -> String {
        match self {
            Self::Chisel => format!("my{accel}"),
            Self::Verilog => format!("{accel}Core"),
        }
    }
}

/// Variables substituted into filenames and `.tmpl` file contents.
#[derive(Debug, Clone)]
pub struct Vars {
    /// User-friendly project name as typed (e.g. "vector-add").
    pub name: String,
    /// Snake-case version, used for scala packages / cmake target /
    /// command function names ("vector_add").
    pub name_snake: String,
    /// PascalCase accelerator name ("VectorAdd"). Defaults to
    /// PascalCase(name); overridable via `--accel`.
    pub accel: String,
    /// AcceleratorSystemConfig name registered in the bindings.
    /// Derived from `accel` per the chosen `Flavor`:
    /// `"my" + accel` for Chisel, `accel + "Core"` for Verilog.
    pub system: String,
    /// Initial deployment target ("default", "aupzu3", ...).
    pub target: String,
    /// Per-target platform-specific TOML block (or empty for
    /// targets we don't have defaults for).
    pub platform_specific: String,
    /// `[hardware.beethoven-hardware]` block in `Beethoven.toml`.
    /// Either `version = "X.Y.Z"` (when `setup` has captured a local
    /// publishLocal) or the legacy `path = "../Beethoven-Hardware"`
    /// fallback (when the user hasn't run setup yet). Pre-rendered so
    /// the template stays a single dumb file with no conditionals.
    pub hardware_dep_toml: String,
    /// Settings injected into `build.sbt` to wire the hardware
    /// dependency. Two shapes:
    ///   - version mode: `libraryDependencies += "<org>" %% "<name>" % "<v>"`
    ///   - path mode: `lazy val beethovenHardware = ProjectRef(...)`
    /// The full sbt project block depends on which one we're in, so
    /// `hardware_sbt_extra_settings` carries the version-mode
    /// `libraryDependencies` line and `hardware_sbt_lazy_val` /
    /// `hardware_sbt_depends_on` carry the path-mode plumbing.
    pub hardware_sbt_lazy_val: String,
    pub hardware_sbt_depends_on: String,
    pub hardware_sbt_extra_settings: String,
}

/// Resolved Beethoven-Hardware coordinates passed to `Vars::new`.
/// `Some(coords)` means setup has captured a published version and
/// the scaffold should resolve via the local Ivy cache; `None` means
/// fall back to a sibling-path source link (legacy framework-dev mode).
#[derive(Debug, Clone)]
pub struct HardwareCoords {
    pub organization: String,
    pub artifact: String,
    pub version: String,
}

impl Vars {
    /// Build the substitution set from the user's name + chosen
    /// target, with optional override of `accel` and optional
    /// hardware coordinates from user config. `flavor` selects the
    /// `{{system}}` naming rule (see [`Flavor`]).
    pub fn new(
        name: &str,
        accel: Option<&str>,
        target: &str,
        hardware: Option<&HardwareCoords>,
        flavor: Flavor,
    ) -> Self {
        let name_snake = to_snake_case(name);
        let accel = accel
            .map(String::from)
            .unwrap_or_else(|| to_pascal_case(name));
        let system = flavor.system_name(&accel);
        let (
            hardware_dep_toml,
            hardware_sbt_lazy_val,
            hardware_sbt_depends_on,
            hardware_sbt_extra_settings,
        ) = render_hardware_blocks(hardware);
        Self {
            name: name.to_string(),
            name_snake,
            accel,
            system,
            target: target.to_string(),
            platform_specific: platform_specific_block(target),
            hardware_dep_toml,
            hardware_sbt_lazy_val,
            hardware_sbt_depends_on,
            hardware_sbt_extra_settings,
        }
    }

    /// Apply substitution to a string. Order of replacements doesn't
    /// matter — the placeholders are all distinct substrings.
    pub fn substitute(&self, s: &str) -> String {
        s.replace("{{name_snake}}", &self.name_snake)
            .replace("{{name}}", &self.name)
            .replace("{{accel}}", &self.accel)
            .replace("{{system}}", &self.system)
            .replace("{{target}}", &self.target)
            .replace("{{platform_specific}}", &self.platform_specific)
            .replace("{{hardware_dep_toml}}", &self.hardware_dep_toml)
            .replace("{{hardware_sbt_lazy_val}}", &self.hardware_sbt_lazy_val)
            .replace("{{hardware_sbt_depends_on}}", &self.hardware_sbt_depends_on)
            .replace(
                "{{hardware_sbt_extra_settings}}",
                &self.hardware_sbt_extra_settings,
            )
    }
}

/// Render the four hardware-related substitution slots based on
/// whether setup has captured a version. Path mode is the legacy
/// fallback; version mode is the post-`beethoven setup` default.
fn render_hardware_blocks(
    coords: Option<&HardwareCoords>,
) -> (String, String, String, String) {
    match coords {
        Some(c) => {
            // Both lines are commented by default — version pinning is
            // opt-in. See Beethoven.toml.tmpl for the user-facing
            // explanation. The matching libraryDependencies in
            // build.sbt is also commented out so the toml stays in sync
            // with what sbt actually resolves.
            let toml = format!(
                "[hardware.beethoven-hardware]\n\
                 # version = \"{}\"          # uncomment to pin to this published version\n\
                 # path = \"../Beethoven-Hardware\"   # uncomment to source-link a sibling checkout\n",
                c.version
            );
            let extra = format!(
                "      // libraryDependencies += \"{}\" %% \"{}\" % \"{}\",\n",
                c.organization, c.artifact, c.version
            );
            (toml, String::new(), String::new(), extra)
        }
        None => {
            let toml = "[hardware.beethoven-hardware]\n\
                        path = \"../Beethoven-Hardware\"\n\
                        # version = \"0.1.7-dev12\"          # uncomment after `beethoven setup`\n"
                .to_string();
            let lazy_val =
                "lazy val beethovenHardware = ProjectRef(file(\"../Beethoven-Hardware\"), \"beethoven\")\n"
                    .to_string();
            let depends_on = "    .dependsOn(beethovenHardware)\n".to_string();
            (toml, lazy_val, depends_on, String::new())
        }
    }
}

/// Lowercase, replace `-` with `_`, drop anything else outside
/// `[a-z0-9_]`. Assumes the input has been validated to start with a
/// letter (caller's job — see `commands::new`).
pub fn to_snake_case(s: &str) -> String {
    s.chars()
        .map(|c| {
            if c == '-' {
                '_'
            } else {
                c.to_ascii_lowercase()
            }
        })
        .filter(|c| c.is_ascii_alphanumeric() || *c == '_')
        .collect()
}

/// Split on `-` / `_` / `.`, drop empty pieces, capitalize the first
/// letter of each remaining piece, lowercase the rest, and concat.
pub fn to_pascal_case(s: &str) -> String {
    s.split(|c: char| c == '-' || c == '_' || c == '.')
        .filter(|p| !p.is_empty())
        .map(|p| {
            let mut chars = p.chars();
            match chars.next() {
                Some(first) => first
                    .to_uppercase()
                    .chain(chars.flat_map(|c| c.to_lowercase()))
                    .collect::<String>(),
                None => String::new(),
            }
        })
        .collect()
}

/// Predefined `[platform.<target>]` block for known targets, or empty
/// if we don't have defaults. Callers (`commands::new`) should warn
/// the user when this returns empty for a non-`"default"` target.
fn platform_specific_block(target: &str) -> String {
    match target {
        "aupzu3" => "[platform.aupzu3]\ndram-size-gb = 8\n".to_string(),
        // default, kria, aws-*, u200 — no defaults baked in. Users can
        // fill in what they need; the `default` target in particular
        // needs nothing.
        _ => String::new(),
    }
}

/// Whether the CLI knows how to scaffold for this target.
pub fn is_known_target(target: &str) -> bool {
    matches!(
        target,
        "default"
            | "kria"
            | "kria2"
            | "aupzu3"
            | "aws-f1"
            | "aws-f2"
            | "u200"
            | "u250"
            | "u280"
            | "baremetal"
    )
}

/// Extract the embedded template tree to `dest`, applying placeholder
/// substitution. The tree is selected by `flavor`. Refuses to overwrite
/// existing files at the destination — callers (commands/new,
/// commands/init) should refuse in advance with a clearer error if they
/// can detect collision earlier.
pub fn extract_to(dest: &Path, vars: &Vars, flavor: Flavor) -> Result<()> {
    fs::create_dir_all(dest).map_err(|e| {
        CliError::config(format!("cannot create {}: {e}", dest.display()))
    })?;
    extract_dir(flavor.tree(), dest, vars)
}

fn extract_dir(dir: &Dir<'_>, dest: &Path, vars: &Vars) -> Result<()> {
    for entry in dir.entries() {
        match entry {
            DirEntry::Dir(d) => {
                let raw_name = d
                    .path()
                    .file_name()
                    .ok_or_else(|| {
                        CliError::config(format!(
                            "template dir without a name: {}",
                            d.path().display()
                        ))
                    })?
                    .to_string_lossy();
                let dst_name = vars.substitute(&raw_name);
                let dst_path = dest.join(&dst_name);
                fs::create_dir_all(&dst_path).map_err(|e| {
                    CliError::config(format!(
                        "cannot create {}: {e}",
                        dst_path.display()
                    ))
                })?;
                extract_dir(d, &dst_path, vars)?;
            }
            DirEntry::File(f) => {
                let raw_name = f
                    .path()
                    .file_name()
                    .ok_or_else(|| {
                        CliError::config(format!(
                            "template file without a name: {}",
                            f.path().display()
                        ))
                    })?
                    .to_string_lossy();
                let mut dst_name = vars.substitute(&raw_name);
                let needs_subst = dst_name.ends_with(".tmpl");
                if needs_subst {
                    dst_name.truncate(dst_name.len() - ".tmpl".len());
                }
                let dst_path = dest.join(&dst_name);

                if dst_path.exists() {
                    return Err(CliError::config(format!(
                        "destination file already exists: {}",
                        dst_path.display()
                    )));
                }

                if needs_subst {
                    let body = std::str::from_utf8(f.contents()).map_err(|e| {
                        CliError::config(format!(
                            "non-UTF8 .tmpl file {}: {e}",
                            f.path().display()
                        ))
                    })?;
                    fs::write(&dst_path, vars.substitute(body)).map_err(|e| {
                        CliError::config(format!(
                            "cannot write {}: {e}",
                            dst_path.display()
                        ))
                    })?;
                } else {
                    fs::write(&dst_path, f.contents()).map_err(|e| {
                        CliError::config(format!(
                            "cannot write {}: {e}",
                            dst_path.display()
                        ))
                    })?;
                }
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn snake_case_basic() {
        assert_eq!(to_snake_case("vector-add"), "vector_add");
        assert_eq!(to_snake_case("Vector-Add"), "vector_add");
        assert_eq!(to_snake_case("VectorAdd"), "vectoradd");
        assert_eq!(to_snake_case("hello_world"), "hello_world");
    }

    #[test]
    fn pascal_case_basic() {
        assert_eq!(to_pascal_case("vector-add"), "VectorAdd");
        assert_eq!(to_pascal_case("vector_add"), "VectorAdd");
        assert_eq!(to_pascal_case("VECTOR-ADD"), "VectorAdd");
        assert_eq!(to_pascal_case("hello"), "Hello");
    }

    #[test]
    fn substitution_idempotent_on_no_placeholders() {
        let v = Vars::new("vector-add", None, "default", None, Flavor::Chisel);
        assert_eq!(v.substitute("plain text"), "plain text");
    }

    #[test]
    fn substitution_replaces_all_vars_chisel() {
        let v = Vars::new("vector-add", None, "aupzu3", None, Flavor::Chisel);
        let out = v.substitute("{{name}}/{{name_snake}}/{{accel}}/{{system}}/{{target}}");
        assert_eq!(out, "vector-add/vector_add/VectorAdd/myVectorAdd/aupzu3");
    }

    #[test]
    fn substitution_replaces_all_vars_verilog() {
        let v = Vars::new("vector-add", None, "default", None, Flavor::Verilog);
        let out = v.substitute("{{name}}/{{name_snake}}/{{accel}}/{{system}}/{{target}}");
        assert_eq!(out, "vector-add/vector_add/VectorAdd/VectorAddCore/default");
    }
}
