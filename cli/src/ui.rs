//! User-facing output helpers — colored prefixes, progress, friendly
//! error printing.
//!
//! All interactive output goes through these so the look stays
//! consistent. Color/TTY detection comes from `console`, which honors
//! `NO_COLOR` and silently disables styling on non-tty stdout/stderr.

use crate::error::CliError;
use console::style;
use std::process::Command;

/// `error: <msg>` to stderr in red, with an optional `hint:` follow-up.
pub fn print_error(err: &CliError) {
    eprintln!("{}: {}", style("error").red().bold(), err);
    if let Some(hint) = err.hint() {
        eprintln!("  {} {}", style("hint:").cyan().bold(), hint);
    }
}

/// `warning: <msg>` in yellow.
pub fn print_warning(msg: &str) {
    eprintln!("{}: {}", style("warning").yellow().bold(), msg);
}

/// `note: <msg>` in cyan.
pub fn print_note(msg: &str) {
    eprintln!("{}: {}", style("note").cyan().bold(), msg);
}

/// `✓ <msg>` in green.
pub fn print_success(msg: &str) {
    println!("{} {}", style("✓").green().bold(), msg);
}

/// Cargo-style stage prefix: a right-aligned 12-char label in green
/// followed by a message. Used for "Building", "Compiling", etc.
pub fn print_stage(label: &str, msg: &str) {
    println!("{:>12} {}", style(label).green().bold(), msg);
}

/// Print `[exec] <envs> <program> <args...>` in blue before running a
/// subprocess. Args are shell-quoted only when they contain
/// non-portable chars, so the printed line is paste-able back into a
/// shell. Env vars set explicitly on the `Command` (via `.env(...)`)
/// are prepended; the rest of the inherited environment is omitted.
pub fn print_exec(cmd: &Command) {
    let mut parts: Vec<String> = cmd
        .get_envs()
        .filter_map(|(k, v)| {
            // get_envs also yields removals (value = None); skip those.
            let v = v?;
            Some(format!(
                "{}={}",
                k.to_string_lossy(),
                shell_quote(&v.to_string_lossy()),
            ))
        })
        .collect();
    parts.push(shell_quote(&cmd.get_program().to_string_lossy()));
    for arg in cmd.get_args() {
        parts.push(shell_quote(&arg.to_string_lossy()));
    }
    println!("{} {}", style("[exec]").blue().bold(), parts.join(" "));
}

/// Quote `s` for safe shell pasting. Returns it verbatim if every
/// character is in the "obviously safe" set; otherwise wraps it in
/// single quotes (escaping any embedded single quotes).
fn shell_quote(s: &str) -> String {
    if s.is_empty() {
        return "''".into();
    }
    let needs_quote = s
        .chars()
        .any(|c| !(c.is_ascii_alphanumeric() || "._-/=:@,+".contains(c)));
    if needs_quote {
        format!("'{}'", s.replace('\'', "'\\''"))
    } else {
        s.to_string()
    }
}

/// Top-of-screen welcome banner — shown on bare `beethoven`, top-level
/// `--help`, or `--version` (see `should_show_banner` in main.rs).
/// `console::style` strips ANSI on non-tty stdout, so piping `--help`
/// to a file or grep produces clean output.
pub fn print_banner() {
    const LINES: &[&str] = &[
        "██████╗ ███████╗███████╗████████╗██╗  ██╗ ██████╗ ██╗   ██╗███████╗███╗   ██╗",
        "██╔══██╗██╔════╝██╔════╝╚══██╔══╝██║  ██║██╔═══██╗██║   ██║██╔════╝████╗  ██║",
        "██████╔╝█████╗  █████╗     ██║   ███████║██║   ██║██║   ██║█████╗  ██╔██╗ ██║",
        "██╔══██╗██╔══╝  ██╔══╝     ██║   ██╔══██║██║   ██║╚██╗ ██╔╝██╔══╝  ██║╚██╗██║",
        "██████╔╝███████╗███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝ ███████╗██║ ╚████║",
        "╚═════╝ ╚══════╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═══╝",
    ];
    for line in LINES {
        println!("{}", style(line).cyan().bold());
    }
    println!(
        "{}",
        style("    FPGA acceleration project orchestrator").dim()
    );
    println!();
}
