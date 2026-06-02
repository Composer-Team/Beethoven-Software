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
    /// AWS F2 helper commands.
    Aws {
        #[command(subcommand)]
        command: AwsCommand,
    },
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

    /// Scaffold a Verilog-blackbox project instead of pure Chisel. The
    /// hardware top is a `<accel>Core.v` file; the framework auto-syncs
    /// its port list against the Scala-side cmd struct on every build.
    #[arg(long)]
    pub verilog: bool,

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

    /// Scaffold a Verilog-blackbox project instead of pure Chisel.
    #[arg(long)]
    pub verilog: bool,

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

    /// Build for synthesis only (target/synthesis/).
    /// Without flags, synthesis-capable targets build both simulation and synthesis.
    #[arg(long, conflicts_with = "simulation")]
    pub release: bool,

    /// Build for simulation only. Useful to skip the synthesis pass
    /// that the default would otherwise run on synthesis-capable targets.
    #[arg(long)]
    pub simulation: bool,

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
    /// SIGKILL the daemon for this project, then wipe its lockfile,
    /// shmem segments, and runtime build dirs. Idempotent.
    Kill(RuntimeKillArgs),
    /// Remove runtime artifacts under target/<mode>/runtime/.
    Clean {
        /// Operate on target/synthesis/runtime/ instead of target/simulation/.
        #[arg(long)]
        release: bool,
    },
}

#[derive(Subcommand, Debug)]
pub enum AwsCommand {
    /// Upload the generated AWS F2 CL package to a remote build machine.
    Upload(AwsUploadArgs),
    /// Create an AWS FPGA image from a completed F2 Vivado build.
    CreateFpgaImage(AwsCreateFpgaImageArgs),
    /// Load an available AWS FPGA image into a local F2 FPGA slot.
    Load(AwsLoadArgs),
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven build hw --release
  beethoven aws upload --host ubuntu@98.81.32.36 --key ~/Desktop/isca-testing.pem

Uploads:
  target/synthesis/aws/cl_beethoven_top/

To:
  <host>:~/cl_beethoven_top/

This command only uploads files. It does not start the long-running AWS/Vivado
build on the remote machine.
")]
pub struct AwsUploadArgs {
    /// SSH destination, e.g. ubuntu@98.81.32.36.
    #[arg(long)]
    pub host: String,

    /// SSH private key passed to ssh via rsync's remote shell.
    #[arg(long)]
    pub key: Option<PathBuf>,

    /// Remote destination directory.
    #[arg(long, default_value = "~/cl_beethoven_top")]
    pub remote_dir: String,

    /// Delete remote files that are absent locally.
    #[arg(long)]
    pub delete: bool,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven aws create-fpga-image --name test-key
  beethoven aws create-fpga-image --cl-dir ~/cl_beethoven_top --bucket beethoven-my-run-123456789012
  beethoven aws create-fpga-image --dry-run

This command is intended to run directly on the AWS F2 build machine after the
Vivado build has finished. It validates timing, uploads the generated
Developer_CL.tar package and design environment tarball to S3, then calls
`aws ec2 create-fpga-image`.
")]
pub struct AwsCreateFpgaImageArgs {
    /// AFI name, description, and S3 object key stem. If omitted, prompts with a suggested default.
    #[arg(long)]
    pub name: Option<String>,

    /// CL directory. Defaults to current directory, or ./cl_beethoven_top if present.
    #[arg(long)]
    pub cl_dir: Option<PathBuf>,

    /// S3 bucket for AFI input tar and logs.
    #[arg(long)]
    pub bucket: Option<String>,

    /// AWS region. Defaults from AWS_REGION, AWS_DEFAULT_REGION, or AWS CLI config.
    #[arg(long)]
    pub region: Option<String>,

    /// Explicitly choose the post-route timing report.
    #[arg(long)]
    pub timing_report: Option<PathBuf>,

    /// Explicitly choose the Developer_CL.tar checkpoint package.
    #[arg(long)]
    pub checkpoint_tar: Option<PathBuf>,

    /// Choose the newest valid artifact when several candidates exist.
    #[arg(long)]
    pub auto: bool,

    /// Print the resolved plan and AWS commands without running them.
    #[arg(long)]
    pub dry_run: bool,

    /// Non-interactive mode; fail instead of prompting, except for generated defaults.
    #[arg(long)]
    pub yes: bool,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven aws load
  beethoven aws load --name cl_beethoven_top-20260602-225900
  beethoven aws load --afi afi-034e1ab10ddc704e9
  beethoven aws load --agfi agfi-01337c0515461e0f2 --slot 0
  beethoven aws load --dry-run

This command only loads an already-created AFI/AGFI into a local AWS F2 FPGA
slot. It does not download the design environment tarball and does not build or
start the Beethoven runtime.
")]
pub struct AwsLoadArgs {
    /// Load a specific AFI, e.g. afi-...
    #[arg(long, conflicts_with = "agfi")]
    pub afi: Option<String>,

    /// Load a specific AGFI directly, e.g. agfi-...
    #[arg(long, conflicts_with = "afi")]
    pub agfi: Option<String>,

    /// Select an available AFI by name.
    #[arg(long, conflicts_with = "agfi")]
    pub name: Option<String>,

    /// AWS region. Defaults from AWS_REGION, AWS_DEFAULT_REGION, or AWS CLI config.
    #[arg(long)]
    pub region: Option<String>,

    /// FPGA slot number.
    #[arg(long, default_value_t = 0)]
    pub slot: u32,

    /// Print the load command without running it.
    #[arg(long)]
    pub dry_run: bool,

    /// Non-interactive mode; fail instead of prompting.
    #[arg(long)]
    pub yes: bool,
}

#[derive(Args, Debug)]
#[command(after_help = "\
EXAMPLES:
  beethoven runtime kill   SIGKILL the daemon and wipe runtime state

After the daemon is gone, also removes:
  - the per-project flock lockfile
  - per-user POSIX shmem segments (/compo_c_<uid>, /compo_d_<uid>)
  - target/simulation/runtime/ and target/synthesis/runtime/ build dirs

Idempotent — if no daemon is running, just runs the cleanup pass.
Resolves the daemon via the per-project lockfile, not pgrep, so it
works the same on Linux and macOS.
")]
pub struct RuntimeKillArgs {}

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

    /// Use an existing Beethoven-Hardware clone instead of letting
    /// setup manage one under ~/.local/share/beethoven/hardware-src.
    /// The provided path is left untouched — only `sbt publishLocal`
    /// runs against it.
    #[arg(long = "hardware-from")]
    pub hardware_from: Option<PathBuf>,

    /// Git ref to check out for Beethoven-Hardware (default: "main").
    /// Ignored with `--hardware-from`.
    #[arg(long = "hardware-ref")]
    pub hardware_ref: Option<String>,

    /// Skip the Beethoven-Hardware install step. Scaffolded projects
    /// will fall back to a sibling-path source link and a manual
    /// `../Beethoven-Hardware` checkout (the legacy default).
    #[arg(long)]
    pub no_hardware: bool,
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
    /// Generic sim-tuned platform — what you pick when you don't care
    /// about a specific FPGA's bus widths/memory params. Renders as
    /// `default` on the CLI and in `Beethoven.toml` to disambiguate
    /// from the build mode `simulation`.
    #[value(name = "default")]
    DefaultTarget,
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
            Self::DefaultTarget => "default",
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
