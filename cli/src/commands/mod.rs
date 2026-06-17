//! Subcommand dispatcher. One handler per file; this module is the
//! single place that knows about every command's existence.

use crate::cli::Command;
use crate::error::Result;

pub mod aws;
pub mod build;
pub mod check;
pub mod clean;
pub mod flash;
pub mod info;
pub mod init;
pub mod new;
pub(crate) mod pipeline;
pub mod run;
pub mod runtime_cmd;
pub mod scaffold;
pub mod setup;
pub mod sim;
pub mod synth;
pub mod uninstall;
pub mod update;

pub fn dispatch(cmd: Command) -> Result<()> {
    match cmd {
        Command::New(args) => new::run(args),
        Command::Init(args) => init::run(args),
        Command::Clean(args) => clean::run(args),
        Command::Check => check::run(),
        Command::Info(args) => info::run(args),
        Command::Build(args) => build::run(args),
        Command::Aws { command } => aws::run(command),
        Command::Runtime { command } => runtime_cmd::run(command),
        Command::Sim(args) => sim::run(args),
        Command::Run(args) => run::run(args),
        Command::Synth(args) => synth::run(args),
        Command::Flash(args) => flash::run(args),
        Command::Setup(args) => setup::run(args),
        Command::Update(args) => update::run(args),
        Command::Uninstall(args) => uninstall::run(args),
    }
}
