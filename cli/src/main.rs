//! Entry point. Tiny by design: parse argv, set up logging, dispatch
//! to a subcommand handler, and translate any error into the right
//! exit code. All real work lives in `cli`, `commands`, and the
//! support modules.

use beethoven_cli::cli::Cli;
use beethoven_cli::{commands, ui};
use clap::Parser;
use std::process::ExitCode;

fn main() -> ExitCode {
    // Show the welcome banner before clap exits with help/version. We
    // limit it to top-level invocations so subcommand `--help` stays
    // tight (e.g. `beethoven sim --help` doesn't repaint the banner).
    if should_show_banner() {
        ui::print_banner();
    }

    let cli = Cli::parse();

    init_tracing(cli.verbose, cli.quiet);

    match commands::dispatch(cli.command) {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            ui::print_error(&err);
            // Exit codes >255 wrap; ours are all in [0, 127] so this fits in u8.
            ExitCode::from(err.exit_code() as u8)
        }
    }
}

/// Show the banner on three "introductory" invocations:
///   - bare `beethoven` (no args)            → triggers help via clap
///   - `beethoven --help` / `-h`            → top-level help
///   - `beethoven --version` / `-V`         → version
///
/// Suppressed on non-tty stdout so piped output stays clean.
fn should_show_banner() -> bool {
    if !console::user_attended() {
        return false;
    }
    let args: Vec<String> = std::env::args().collect();
    match args.len() {
        1 => true,
        2 => matches!(args[1].as_str(), "--help" | "-h" | "--version" | "-V"),
        _ => false,
    }
}

/// Configure tracing filter from the `-v` / `-q` flags. Subscriber
/// writes to stderr so it never pollutes stdout, which is reserved
/// for the subcommand's own output (greppable, pipeable).
fn init_tracing(verbose: u8, quiet: bool) {
    use tracing_subscriber::EnvFilter;

    let directive = if quiet {
        "warn"
    } else {
        match verbose {
            0 => "info",
            1 => "debug",
            _ => "trace",
        }
    };
    let filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new(directive));

    tracing_subscriber::fmt()
        .with_env_filter(filter)
        .with_writer(std::io::stderr)
        .with_target(false)
        .without_time()
        .init();
}
