//! Clap derive tree for the `beethoven` CLI.
//!
//! One enum variant per subcommand, mirroring `cli/docs/*.md`. The
//! dispatcher in `commands::dispatch` routes each variant to a handler.

use clap::{Args, Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(
    name = "beethoven",
    version,
    about = "Orchestrate Beethoven hardware acceleration projects",
    long_about = None,
    arg_required_else_help = true,
)]
pub struct Cli {
    /// Verbose output (`-vv` for trace).
    #[arg(short, long, action = clap::ArgAction::Count, global = true)]
    pub verbose: u8,

    /// Suppress non-error output.
    #[arg(short, long, global = true, conflicts_with = "verbose")]
    pub quiet: bool,

    #[command(subcommand)]
    pub command: Command,
}

#[derive(Subcommand, Debug)]
pub enum Command {
    /// Scaffold a new project in a new directory.
    New(NewArgs),
    /// Scaffold in the current directory.
    Init(InitArgs),
    /// Remove build artifacts under target/.
    Clean(CleanArgs),
    /// Validate Beethoven.toml and elaborate the chisel design (no codegen).
    Check,
    /// Print the resolved configuration.
    Info(InfoArgs),
    /// Build hw, runtime, and user sw — sliced as `build {hw|runtime|sw}`.
    Build(BuildArgs),
    /// Manage the runtime daemon: build, run, or clean.
    Runtime {
        #[command(subcommand)]
        command: RuntimeCommand,
    },
    /// Build and run end-to-end in simulation. Auto-detects an existing daemon.
    /// Extra args after `--` are forwarded to the testbench, like `cargo run`.
    Sim(SimArgs),
    /// Build and run end-to-end on real FPGA. Auto-detects an existing daemon.
    /// Extra args after `--` are forwarded to the testbench, like `cargo run`.
    Run(RunArgs),
    /// Drive Vivado: setup + synth + impl + bitstream. Always full rebuild.
    Synth(SynthArgs),
    /// JTAG-program the FPGA with the latest bitstream.
    Flash(FlashArgs),
    /// First-run bootstrap: clone Beethoven-Software and install libbeethoven.
    Setup(SetupArgs),
    /// Pull and reinstall libbeethoven.
    Update(UpdateArgs),
    /// Remove libbeethoven from the install prefix.
    Uninstall(UninstallArgs),
}

#[derive(Args, Debug)]
pub struct NewArgs {
    /// Project name (also the directory name).
    pub name: String,

    /// Target platform.
    #[arg(long, value_enum)]
    pub platform: Option<Platform>,

    /// Accelerator class name (defaults to PascalCased project name).
    #[arg(long)]
    pub accel: Option<String>,

    /// Initialize a git repository and make an initial commit.
    #[arg(long)]
    pub vcs: bool,
}

#[derive(Args, Debug)]
pub struct InitArgs {
    /// Target platform.
    #[arg(long, value_enum)]
    pub platform: Option<Platform>,

    /// Accelerator class name (defaults to PascalCased directory name).
    #[arg(long)]
    pub accel: Option<String>,

    /// Initialize a git repository and make an initial commit.
    #[arg(long)]
    pub vcs: bool,
}

#[derive(Args, Debug)]
pub struct CleanArgs {
    /// Remove target/synthesis/ instead of target/simulation/.
    #[arg(long)]
    pub release: bool,

    /// Remove the entire target/ directory.
    #[arg(long)]
    pub all: bool,
}

#[derive(Args, Debug)]
pub struct InfoArgs {
    /// Output format.
    #[arg(long, value_enum, default_value_t = OutputFormat::Text)]
    pub format: OutputFormat,
}

#[derive(Args, Debug)]
pub struct BuildArgs {
    /// Which target to build (defaults to all three).
    #[arg(value_enum)]
    pub target: Option<BuildTarget>,

    /// Build for synthesis (target/synthesis/) instead of simulation.
    #[arg(long)]
    pub release: bool,

    /// Number of parallel jobs.
    #[arg(short = 'j')]
    pub jobs: Option<usize>,
}

#[derive(Subcommand, Debug)]
pub enum RuntimeCommand {
    /// Cmake-build the runtime daemon.
    Build {
        /// Build for synthesis instead of simulation.
        #[arg(long)]
        release: bool,
        /// Number of parallel jobs.
        #[arg(short = 'j')]
        jobs: Option<usize>,
    },
    /// Launch the daemon in foreground (Ctrl+C to stop).
    Run(RuntimeRunArgs),
    /// Remove runtime artifacts under target/<mode>/runtime/.
    Clean {
        /// Operate on target/synthesis/runtime/ instead of target/simulation/.
        #[arg(long)]
        release: bool,
    },
}

#[derive(Args, Debug)]
pub struct RuntimeRunArgs {
    /// Run the synth-mode daemon.
    #[arg(long)]
    pub release: bool,

    /// Tee daemon stderr to a file.
    #[arg(long)]
    pub log_file: Option<PathBuf>,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven sim                        run the only testbench in simulation
  beethoven sim mytb                   run a specific testbench
  beethoven sim --no-build             skip the rebuild step
  beethoven sim --simulator verilator  override simulator backend
  beethoven sim mytb -- --foo bar      forward `--foo bar` to the testbench
  beethoven sim -- --foo bar           same, when there's only one testbench
")]
pub struct SimArgs {
    /// Which testbench to run (defaults to the only one in Beethoven.toml).
    pub testbench: Option<String>,

    /// Skip the rebuild step.
    #[arg(long)]
    pub no_build: bool,

    /// Refuse to launch a daemon; require one to be already up.
    #[arg(long)]
    pub no_launch: bool,

    /// Override the simulator backend.
    #[arg(long)]
    pub simulator: Option<String>,

    /// Arguments forwarded to the testbench. Must follow `--`, like
    /// `cargo run -- --foo bar`.
    #[arg(last = true, allow_hyphen_values = true)]
    pub tb_args: Vec<String>,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven run                        run the only testbench on real FPGA
  beethoven run mytb                   run a specific testbench
  beethoven run --no-build             skip the rebuild step
  beethoven run mytb -- --foo bar      forward `--foo bar` to the testbench
  beethoven run -- --foo bar           same, when there's only one testbench

NOTE: real-FPGA runs need /dev/mem access — invoke under sudo, e.g.:
  sudo -E env HOME=$HOME beethoven run -- --foo bar
")]
pub struct RunArgs {
    /// Which testbench to run (defaults to the only one built).
    pub testbench: Option<String>,

    /// Skip the rebuild step.
    #[arg(long)]
    pub no_build: bool,

    /// Refuse to launch a daemon; require one to be already up.
    #[arg(long)]
    pub no_launch: bool,

    /// Arguments forwarded to the testbench. Must follow `--`, like
    /// `cargo run -- --foo bar`.
    #[arg(last = true, allow_hyphen_values = true)]
    pub tb_args: Vec<String>,
}

#[derive(Args, Debug)]
pub struct SetupArgs {
    /// Where to install libbeethoven (default: ~/.local).
    #[arg(long)]
    pub prefix: Option<PathBuf>,

    /// Git ref to check out (default: "main"). Ignored with `--from`.
    #[arg(long = "ref")]
    pub git_ref: Option<String>,

    /// Use an existing local Beethoven-Software clone instead of cloning.
    /// The provided path is left untouched (the build dir stays in /tmp).
    #[arg(long)]
    pub from: Option<PathBuf>,

    /// Number of parallel build jobs (default: all cores).
    #[arg(short = 'j')]
    pub jobs: Option<usize>,
}

#[derive(Args, Debug)]
pub struct UpdateArgs {
    /// Switch to a different git ref.
    #[arg(long = "ref")]
    pub git_ref: Option<String>,

    /// Use an existing local Beethoven-Software clone instead of cloning.
    #[arg(long)]
    pub from: Option<PathBuf>,

    /// Number of parallel build jobs (default: all cores).
    #[arg(short = 'j')]
    pub jobs: Option<usize>,
}

#[derive(Args, Debug)]
pub struct UninstallArgs {
    /// Skip the confirmation prompt.
    #[arg(long, short = 'y')]
    pub yes: bool,
}

#[derive(ValueEnum, Clone, Copy, Debug)]
pub enum BuildTarget {
    Hw,
    Runtime,
    Sw,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven synth                   full pipeline: setup -> synth -> impl -> bit
  beethoven synth --no-build        skip the chisel `build hw --release` step
  beethoven synth --up-to synth     stop after synth_design (skip impl + bit)
  beethoven synth --up-to impl      stop after impl (skip bit)
  beethoven synth --gui             open Vivado GUI on 0_setup.tcl, then exit

NOTE: every run starts from a clean `xilinx_work/` (0_setup.tcl wipes it).
      Use `beethoven flash` to JTAG-program the produced bitstream.
")]
pub struct SynthArgs {
    /// Skip the prerequisite `beethoven build hw --release` step.
    #[arg(long)]
    pub no_build: bool,

    /// Stop after this stage. Default: bit (full pipeline).
    #[arg(long, value_enum, value_name = "STAGE")]
    pub up_to: Option<SynthStage>,

    /// Open Vivado GUI on 0_setup.tcl and exit. Does not run synth/impl.
    #[arg(long, conflicts_with_all = ["no_build", "up_to"])]
    pub gui: bool,
}

#[derive(ValueEnum, Clone, Copy, Debug, PartialEq, Eq)]
pub enum SynthStage {
    /// Just the block design + IP setup.
    Setup,
    /// Through synth_design.
    Synth,
    /// Through place + route.
    Impl,
    /// Through bitstream (default).
    Bit,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven flash    JTAG-program the FPGA with the bitstream produced by `synth`

NOTE: requires Xilinx hw_server reachable at localhost:3121 (the JTAG TCL
      defaults to that). Run from the same machine the cable is attached to.
")]
pub struct FlashArgs {}

#[derive(ValueEnum, Clone, Copy, Debug)]
pub enum Platform {
    Simulation,
    Kria,
    Kria2,
    Aupzu3,
    AwsF1,
    AwsF2,
    U200,
    U250,
    U280,
    Baremetal,
}

impl Platform {
    /// Stringified target name as it appears in `Beethoven.toml`'s
    /// `[platform].target` field. Matches the kebab-case used by clap
    /// for `--platform` arguments.
    pub fn target_name(&self) -> &'static str {
        match self {
            Self::Simulation => "simulation",
            Self::Kria => "kria",
            Self::Kria2 => "kria2",
            Self::Aupzu3 => "aupzu3",
            Self::AwsF1 => "aws-f1",
            Self::AwsF2 => "aws-f2",
            Self::U200 => "u200",
            Self::U250 => "u250",
            Self::U280 => "u280",
            Self::Baremetal => "baremetal",
        }
    }
}

#[derive(ValueEnum, Clone, Copy, Debug)]
pub enum OutputFormat {
    Text,
    Json,
}
