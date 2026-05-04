//! `beethoven info` — print resolved configuration.
//!
//! Three sections, each its own table:
//!   - User config (prefix, ref, default platform)
//!   - libbeethoven (which libs found, runtime-src dir)
//!   - Project (name, version, target, build mode, daemon status)
//!
//! Outside a project the third section is omitted.

use crate::cli::{InfoArgs, OutputFormat};
use crate::core::env;
use crate::error::Result;
use crate::runtime::probe::{self, ProbeResult};
use crate::state::{Project, UserConfig};
use crate::tools::cmake;
use comfy_table::{presets::NOTHING, Cell, CellAlignment, Table};
use console::style;
use serde_json::{json, Value};
use std::path::PathBuf;

pub fn run(args: InfoArgs) -> Result<()> {
    let user = UserConfig::load()?;
    let project = Project::discover().ok();
    let installed = collect_installed(&user)?;
    let daemon = collect_daemon(project.as_ref());

    match args.format {
        OutputFormat::Text => print_text(&user, &installed, project.as_ref(), &daemon),
        OutputFormat::Json => print_json(&user, &installed, project.as_ref(), &daemon),
    }
    Ok(())
}

#[derive(Debug, Default)]
struct InstalledInfo {
    libs: Vec<PathBuf>,
    runtime_src: Option<PathBuf>,
}

#[derive(Debug)]
enum DaemonStatus {
    NotInProject,
    Down,
    Up { pid: Option<i32> },
    Error,
}

fn collect_installed(user: &UserConfig) -> Result<InstalledInfo> {
    let mut info = InstalledInfo::default();
    if let Some(prefix) = &user.prefix {
        let manifest = env::manifest_path()?;
        if manifest.is_file() {
            info.libs = cmake::find_installed_libs(&manifest);
        }
        info.runtime_src = cmake::parse_runtime_src_dir(prefix);
    }
    Ok(info)
}

fn collect_daemon(project: Option<&Project>) -> DaemonStatus {
    match project {
        None => DaemonStatus::NotInProject,
        Some(p) => match probe::probe(&p.root) {
            Ok(ProbeResult::Down) => DaemonStatus::Down,
            Ok(ProbeResult::Up { pid }) => DaemonStatus::Up { pid },
            Ok(ProbeResult::Error(_)) | Err(_) => DaemonStatus::Error,
        },
    }
}

// ---------- text output ----------

fn print_text(
    user: &UserConfig,
    installed: &InstalledInfo,
    project: Option<&Project>,
    daemon: &DaemonStatus,
) {
    print_section("User config");
    print_kv_table(&[
        (
            "prefix",
            user.prefix
                .as_ref()
                .map_or("(unset; run `beethoven setup`)".into(), |p| {
                    p.display().to_string()
                }),
        ),
        (
            "ref",
            user.git_ref.clone().unwrap_or_else(|| "(unset)".into()),
        ),
        (
            "default platform",
            user.default_platform
                .clone()
                .unwrap_or_else(|| "(unset)".into()),
        ),
    ]);

    println!();
    print_section("libbeethoven");
    if installed.libs.is_empty() && installed.runtime_src.is_none() {
        println!(
            "  {}",
            style("(not installed; run `beethoven setup`)").dim()
        );
    } else {
        let mut rows: Vec<(&str, String)> = Vec::new();
        for lib in &installed.libs {
            rows.push(("lib", lib.display().to_string()));
        }
        if let Some(rt) = &installed.runtime_src {
            rows.push(("runtime-src", rt.display().to_string()));
        }
        print_kv_table(&rows);
    }

    if let Some(p) = project {
        println!();
        print_section("Project");
        let mut rows: Vec<(&str, String)> = vec![
            ("name", p.manifest.project.name.clone()),
            ("root", p.root.display().to_string()),
        ];
        match p.target() {
            Some(t) => {
                let resolved = p.platform().map_or("?".into(), |pl| pl.as_str().to_string());
                rows.push(("target", format!("{t}  →  {resolved}")));
            }
            None => rows.push(("target", "(unset)".into())),
        }
        if let Some(spec) = p.target_specific() {
            if !spec.is_empty() {
                let line = spec
                    .iter()
                    .map(|(k, v)| format!("{k} = {}", render_toml_value(v)))
                    .collect::<Vec<_>>()
                    .join(", ");
                rows.push(("platform config", line));
            }
        }
        if let Some(path) = p.beethoven_hardware_path() {
            rows.push(("hw dep (path)", path.to_string()));
        } else if let Some(ver) = p.beethoven_hardware_version() {
            rows.push(("hw dep (version)", ver.to_string()));
        } else {
            rows.push(("hw dep", "(unset; uncomment in Beethoven.toml)".into()));
        }
        let daemon_str = match daemon {
            DaemonStatus::Up { pid: Some(pid) } => format!("up (PID {pid})"),
            DaemonStatus::Up { pid: None } => "up".into(),
            DaemonStatus::Down => "down".into(),
            DaemonStatus::Error => "(probe error)".into(),
            DaemonStatus::NotInProject => "(n/a)".into(),
        };
        rows.push(("daemon", daemon_str));
        print_kv_table(&rows);
    }
}

fn render_toml_value(v: &toml::Value) -> String {
    match v {
        toml::Value::String(s) => format!("\"{s}\""),
        toml::Value::Integer(i) => i.to_string(),
        toml::Value::Float(f) => f.to_string(),
        toml::Value::Boolean(b) => b.to_string(),
        other => other.to_string(),
    }
}

fn print_section(title: &str) {
    println!("{}", style(title).cyan().bold());
}

fn print_kv_table(rows: &[(&str, String)]) {
    let mut t = Table::new();
    t.load_preset(NOTHING);
    for (k, v) in rows {
        t.add_row(vec![
            Cell::new(k).set_alignment(CellAlignment::Right),
            Cell::new(v),
        ]);
    }
    println!("{t}");
}

// ---------- json output ----------

fn print_json(
    user: &UserConfig,
    installed: &InstalledInfo,
    project: Option<&Project>,
    daemon: &DaemonStatus,
) {
    let user_json = json!({
        "prefix": user.prefix.as_ref().map(|p| p.display().to_string()),
        "ref": user.git_ref.clone(),
        "default_platform": user.default_platform.clone(),
    });
    let installed_json = json!({
        "libs": installed.libs.iter().map(|p| p.display().to_string()).collect::<Vec<_>>(),
        "runtime_src_dir": installed.runtime_src.as_ref().map(|p| p.display().to_string()),
    });
    let project_json: Value = match project {
        None => Value::Null,
        Some(proj) => {
            let target = proj.target();
            let platform = proj.platform().map(|p| p.as_str());
            let target_specific = proj.target_specific().map(|t| {
                serde_json::to_value(t).unwrap_or(Value::Null)
            });
            let daemon_json = match daemon {
                DaemonStatus::Up { pid } => json!({"up": true, "pid": pid}),
                DaemonStatus::Down => json!({"up": false, "pid": Value::Null}),
                _ => json!({"up": Value::Null, "pid": Value::Null}),
            };
            json!({
                "name": &proj.manifest.project.name,
                "root": proj.root.display().to_string(),
                "target": target,
                "platform": platform,
                "target_specific": target_specific,
                "hw_dep_path": proj.beethoven_hardware_path(),
                "hw_dep_version": proj.beethoven_hardware_version(),
                "daemon": daemon_json,
            })
        }
    };
    let out = json!({
        "user": user_json,
        "installed": installed_json,
        "project": project_json,
    });
    println!("{}", serde_json::to_string_pretty(&out).unwrap());
}
