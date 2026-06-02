//! `beethoven build [TARGET]` — orchestrates the three artifacts:
//! hardware (sbt), runtime daemon (cmake), user testbench (cmake).
//!
//! Each subgroup is strict — it rejects rather than implicitly building
//! prereqs. Bare `build` runs all three in dependency order. The
//! per-target functions are `pub fn` so `runtime build` / `sim` /
//! `run` can re-use them.

use crate::cli::{BuildArgs, BuildTarget};
use crate::core::exec;
use crate::error::{CliError, Result};
use crate::state::{target_supports_synth, Project, UserConfig};
use crate::tools::{cmake, sbt};
use crate::ui;
use std::env;
use std::path::PathBuf;

pub fn run(args: BuildArgs) -> Result<()> {
    let project = Project::discover()?;
    let mode = if args.release {
        "synthesis"
    } else {
        "simulation"
    };
    let synth_ok = project.target().map_or(true, target_supports_synth);

    if args.release && !synth_ok {
        return Err(CliError::config(format!(
            "target = \"{}\" has no synthesis flow; remove --release",
            project.target().unwrap_or("?")
        )));
    }

    match args.target {
        // Default builds both modes for real-FPGA targets — chisel emits
        // different RTL per mode (sim harness vs synth-side floorplan /
        // timing knobs), and most workflows need both. `--release` /
        // `--simulation` opt into a single mode. The generic `default`
        // target has no synth flow, so we skip the synth pass there.
        Some(BuildTarget::Hw) => {
            for mode in selected_modes(&args, synth_ok) {
                build_hw(&project, mode)?;
            }
        }
        Some(BuildTarget::Runtime) => build_runtime(&project, mode, args.jobs, None)?,
        Some(BuildTarget::Sw) => build_sw(&project, args.jobs)?,
        None => {
            for mode in selected_modes(&args, synth_ok) {
                build_hw(&project, mode)?;
            }
            for mode in selected_modes(&args, synth_ok) {
                build_runtime(&project, mode, args.jobs, None)?;
            }
            build_sw(&project, args.jobs)?;
        }
    }

    ui::print_success("build complete.");
    Ok(())
}

fn selected_modes(args: &BuildArgs, synth_ok: bool) -> &'static [&'static str] {
    if args.release {
        &["synthesis"]
    } else if args.simulation || !synth_ok {
        &["simulation"]
    } else {
        &["simulation", "synthesis"]
    }
}

/// Hardware step: `sbt "run --mode <mode>"` from the project root,
/// then verify `target/binding/beethoven_hardware.cc` landed.
///
/// `mode` is required because the chisel elaboration emits different
/// RTL per mode (sim harness, synth-side floorplan/timing knobs,
/// etc.) — see Beethoven-Hardware's Manifest.scala for the rationale
/// for keeping mode out of the manifest entirely.
pub fn build_hw(project: &Project, mode: &str) -> Result<()> {
    exec::require_tool("sbt", Some("install via your package manager (sbt 1.x)"))?;

    ui::print_stage(
        "Generating",
        &format!("verilog + bindings (sbt run --mode {mode})"),
    );
    sbt::run(&project.root, mode)?;

    let bindings = bindings_path(project);
    if !bindings.is_file() {
        return Err(CliError::config(format!(
            "sbt completed but bindings are missing at {}.\n\
             Check sbt output for chisel-elaboration errors.",
            bindings.display()
        )));
    }
    Ok(())
}

/// Runtime step: cmake against the installed runtime source-package.
/// Strict — refuses without bindings or libbeethoven.
///
/// `simulator_override` lets `sim` plumb its `--simulator` flag down
/// to the cmake invocation; existing callers (`build`, `runtime
/// build`) pass `None` and fall back to the manifest's `simulator`
/// (default `verilator`).
pub fn build_runtime(
    project: &Project,
    mode: &str,
    jobs: Option<usize>,
    simulator_override: Option<&str>,
) -> Result<()> {
    require_bindings(project)?;
    let runtime_src = require_runtime_src()?;
    exec::require_tool("cmake", Some("install via your package manager"))?;

    let target = project
        .target()
        .ok_or_else(|| CliError::config("[platform].target is required to build the runtime"))?;
    let platform = project.platform().ok_or_else(|| {
        CliError::config(format!(
            "unrecognized target '{target}'; cannot resolve platform"
        ))
    })?;
    require_aws_f2_runtime_env(target, mode)?;

    let project_root_str = project.root.display().to_string();
    let mut defines: Vec<(&str, &str)> = vec![
        ("BEETHOVEN_PROJECT_ROOT", project_root_str.as_str()),
        ("BEETHOVEN_BUILD_MODE", mode),
        ("BEETHOVEN_TARGET", target),
        ("BEETHOVEN_PLATFORM", platform.as_str()),
        ("CMAKE_BUILD_TYPE", "Release"),
    ];
    let simulator = simulator_override.unwrap_or_else(|| project.simulator());
    if mode == "simulation" {
        defines.push(("BEETHOVEN_SIMULATOR", simulator));
    }

    let build_dir = project
        .root
        .join("target")
        .join(mode)
        .join("runtime")
        .join("_cmake");

    ui::print_stage(
        "Configuring",
        &format!("runtime ({mode}, {})", platform.as_str()),
    );
    cmake::configure(&runtime_src, &build_dir, &defines)?;

    ui::print_stage("Building", "runtime daemon");
    cmake::build(&build_dir, jobs)?;

    Ok(())
}

fn require_aws_f2_runtime_env(target: &str, mode: &str) -> Result<()> {
    if target != "aws-f2" || mode != "synthesis" {
        return Ok(());
    }

    let aws_fpga_repo_dir = env::var("AWS_FPGA_REPO_DIR").map_err(|_| {
        CliError::config(
            "aws-f2 release runtime build requires the AWS FPGA SDK environment.\n\
             Run:\n\
               cd ~/aws-fpga\n\
               source sdk_setup.sh\n\
               cd -\n\
             Then retry `beethoven runtime build --release`.",
        )
    })?;

    let sdk_include = PathBuf::from(&aws_fpga_repo_dir)
        .join("sdk")
        .join("userspace")
        .join("include");
    if !sdk_include.is_dir() {
        return Err(CliError::config(format!(
            "AWS_FPGA_REPO_DIR is set to {}, but {} does not exist.\n\
             Source the correct aws-fpga SDK first:\n\
               cd ~/aws-fpga\n\
               source sdk_setup.sh",
            aws_fpga_repo_dir,
            sdk_include.display()
        )));
    }

    Ok(())
}

/// User-sw step: cmake against the project's `sw/` directory. Strict
/// — refuses without libbeethoven and without bindings (the
/// `beethoven_build` cmake macro pulls in the binding `.cc` file).
///
/// Note: sw is *mode-agnostic* (only the platform affects the build),
/// so the build dir is `target/sw/`, not `target/<mode>/sw/`.
pub fn build_sw(project: &Project, jobs: Option<usize>) -> Result<()> {
    require_libbeethoven_installed()?;
    require_bindings(project)?;
    exec::require_tool("cmake", Some("install via your package manager"))?;

    let target = project
        .target()
        .ok_or_else(|| CliError::config("[platform].target is required to build sw"))?;
    let platform = project
        .platform()
        .ok_or_else(|| CliError::config(format!("unrecognized target '{target}'")))?;

    let project_root_str = project.root.display().to_string();
    let defines: Vec<(&str, &str)> = vec![
        ("BEETHOVEN_PROJECT_ROOT", project_root_str.as_str()),
        ("BEETHOVEN_PLATFORM", platform.as_str()),
        ("CMAKE_BUILD_TYPE", "Release"),
    ];

    let sw_src = project.root.join("sw");
    let build_dir = project.root.join("target").join("sw");

    ui::print_stage("Configuring", &format!("sw ({})", platform.as_str()));
    cmake::configure(&sw_src, &build_dir, &defines)?;

    ui::print_stage("Building", "user testbench");
    cmake::build(&build_dir, jobs)?;

    Ok(())
}

// ---- prerequisite checks ----

fn bindings_path(project: &Project) -> PathBuf {
    project
        .root
        .join("target")
        .join("binding")
        .join("beethoven_hardware.cc")
}

// ---- artifact-presence predicates (used by `sim` / `run` to decide
//      whether to skip the build) ----

/// True if the chisel-generated bindings AND the per-mode verilog
/// directory both exist. Bindings are mode-shared but mode-specific
/// verilog lives at `target/<mode>/hw/`, so we need both for a given
/// `mode` to be considered "hw built".
pub fn hw_already_built(project: &Project, mode: &str) -> bool {
    if !bindings_path(project).is_file() {
        return false;
    }
    let mode_hw = project.root.join("target").join(mode).join("hw");
    if !mode_hw.is_dir() {
        return false;
    }
    // Any .v / .sv file inside is enough.
    match std::fs::read_dir(&mode_hw) {
        Ok(entries) => entries.flatten().any(|e| {
            e.path()
                .extension()
                .and_then(|x| x.to_str())
                .map_or(false, |x| x == "v" || x == "sv")
        }),
        Err(_) => false,
    }
}

fn require_bindings(project: &Project) -> Result<()> {
    let bindings = bindings_path(project);
    if !bindings.is_file() {
        return Err(CliError::config(format!(
            "missing bindings at {}.\n\
             Run `beethoven build hw` first.",
            bindings.display()
        )));
    }
    Ok(())
}

fn require_libbeethoven_installed() -> Result<()> {
    let cfg = UserConfig::load()?;
    if cfg.prefix.is_none() {
        return Err(CliError::config(
            "libbeethoven is not installed; run `beethoven setup` first.".to_string(),
        ));
    }
    Ok(())
}

fn require_runtime_src() -> Result<PathBuf> {
    let cfg = UserConfig::load()?;
    let prefix = cfg.prefix.ok_or_else(|| {
        CliError::config("libbeethoven is not installed; run `beethoven setup` first.".to_string())
    })?;
    cmake::parse_runtime_src_dir(&prefix).ok_or_else(|| {
        CliError::config(format!(
            "cannot find BEETHOVEN_RUNTIME_SRC_DIR under {} — \
             reinstall libbeethoven with `beethoven update`.",
            prefix.display()
        ))
    })
}

#[cfg(test)]
mod tests {
    use super::selected_modes;
    use crate::cli::BuildArgs;

    fn args(release: bool, simulation: bool) -> BuildArgs {
        BuildArgs {
            target: None,
            release,
            simulation,
            jobs: None,
        }
    }

    #[test]
    fn default_build_uses_both_modes_when_synth_is_supported() {
        assert_eq!(
            selected_modes(&args(false, false), true),
            &["simulation", "synthesis"]
        );
    }

    #[test]
    fn default_build_stays_simulation_only_when_synth_is_unsupported() {
        assert_eq!(selected_modes(&args(false, false), false), &["simulation"]);
    }

    #[test]
    fn release_forces_synthesis_only() {
        assert_eq!(selected_modes(&args(true, false), true), &["synthesis"]);
    }

    #[test]
    fn simulation_flag_forces_simulation_only() {
        assert_eq!(selected_modes(&args(false, true), true), &["simulation"]);
    }
}
