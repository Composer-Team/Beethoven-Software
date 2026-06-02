//! `beethoven aws ...` — AWS F2 helper commands.
//!
//! Keep long-running remote work explicit and outside this CLI for now:
//! `upload` only synchronizes the locally generated CL package.

use crate::cli::{AwsCommand, AwsCreateFpgaImageArgs, AwsUploadArgs};
use crate::core::exec;
use crate::error::{CliError, Result};
use crate::state::Project;
use crate::ui;
use dialoguer::{Input, Select};
use serde::Deserialize;
use std::ffi::OsStr;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{SystemTime, UNIX_EPOCH};
use walkdir::WalkDir;

pub fn run(command: AwsCommand) -> Result<()> {
    match command {
        AwsCommand::Upload(args) => upload(args),
        AwsCommand::CreateFpgaImage(args) => create_fpga_image(args),
    }
}

fn create_fpga_image(args: AwsCreateFpgaImageArgs) -> Result<()> {
    exec::require_tool(
        "aws",
        Some("install the AWS CLI and configure AWS credentials"),
    )?;
    exec::require_tool("tar", Some("install tar and ensure it is on PATH"))?;

    let cl_dir = resolve_cl_dir(args.cl_dir)?;
    validate_cl_dir(&cl_dir)?;

    let timing_report =
        resolve_timing_report(args.timing_report.as_deref(), &cl_dir, args.auto, args.yes)?;
    validate_timing_report(&timing_report)?;

    let checkpoint_tar =
        resolve_checkpoint_tar(args.checkpoint_tar.as_deref(), &cl_dir, args.auto, args.yes)?;
    let name = resolve_name(args.name.as_deref(), &cl_dir, args.yes)?;
    let region = resolve_region(args.region.as_deref(), args.yes)?;
    let bucket = resolve_bucket(args.bucket.as_deref(), &name, args.yes)?;

    let env_tar = cl_dir.join(format!("env_{name}.tar.gz"));
    let plan = CreateFpgaImagePlan {
        cl_dir,
        timing_report,
        checkpoint_tar,
        name,
        region,
        bucket,
        env_tar,
    };

    if args.dry_run {
        print_dry_run(&plan);
        return Ok(());
    }

    execute_create_fpga_image(&plan)
}

fn upload(args: AwsUploadArgs) -> Result<()> {
    let project = Project::discover()?;
    require_aws_f2(&project)?;
    exec::require_tool("rsync", Some("install rsync and ensure it is on PATH"))?;
    exec::require_tool(
        "ssh",
        Some("install OpenSSH client and ensure ssh is on PATH"),
    )?;

    let package_dir = aws_f2_package_dir(&project);
    if !package_dir.is_dir() {
        return Err(CliError::config(format!(
            "missing AWS F2 package at {}.\n\
             Run `beethoven build hw --release` first.",
            package_dir.display()
        )));
    }
    require_file(&package_dir.join("build_beethoven_f2.sh"))?;
    require_dir(&package_dir.join("design"))?;
    require_dir(&package_dir.join("build").join("scripts"))?;
    require_dir(&package_dir.join("build").join("constraints"))?;

    let mut cmd = Command::new("rsync");
    cmd.arg("-avz");
    if args.delete {
        cmd.arg("--delete");
    }
    if let Some(key) = args.key {
        cmd.arg("-e")
            .arg(format!("ssh -i {}", shell_quote_path(&key)));
    }
    cmd.arg(format!("{}/", package_dir.display())).arg(format!(
        "{}:{}/",
        args.host,
        args.remote_dir.trim_end_matches('/')
    ));

    ui::print_stage("Uploading", "AWS F2 CL package");
    exec::run(&mut cmd)?;
    ui::print_success(&format!(
        "uploaded {} to {}:{}/",
        package_dir.display(),
        args.host,
        args.remote_dir.trim_end_matches('/')
    ));
    Ok(())
}

fn require_aws_f2(project: &Project) -> Result<()> {
    match project.target() {
        Some("aws-f2") => Ok(()),
        Some(other) => Err(CliError::config(format!(
            "`beethoven aws upload` requires [platform].target = \"aws-f2\"; found \"{other}\""
        ))),
        None => Err(CliError::config(
            "`beethoven aws upload` requires [platform].target = \"aws-f2\"".to_string(),
        )),
    }
}

fn aws_f2_package_dir(project: &Project) -> PathBuf {
    project
        .root
        .join("target")
        .join("synthesis")
        .join("aws")
        .join("cl_beethoven_top")
}

fn require_dir(path: &Path) -> Result<()> {
    if path.is_dir() {
        Ok(())
    } else {
        Err(CliError::config(format!(
            "incomplete AWS F2 package: missing directory {}",
            path.display()
        )))
    }
}

fn require_file(path: &Path) -> Result<()> {
    if path.is_file() {
        Ok(())
    } else {
        Err(CliError::config(format!(
            "incomplete AWS F2 package: missing file {}",
            path.display()
        )))
    }
}

fn shell_quote_path(path: &Path) -> String {
    let s = path.display().to_string();
    if s.is_empty() {
        return "''".to_string();
    }
    let needs_quote = s
        .chars()
        .any(|c| !(c.is_ascii_alphanumeric() || "._-/=:@,+~".contains(c)));
    if needs_quote {
        format!("'{}'", s.replace('\'', "'\\''"))
    } else {
        s
    }
}

struct CreateFpgaImagePlan {
    cl_dir: PathBuf,
    timing_report: PathBuf,
    checkpoint_tar: PathBuf,
    name: String,
    region: String,
    bucket: String,
    env_tar: PathBuf,
}

#[derive(Deserialize)]
#[serde(rename_all = "PascalCase")]
struct CreateFpgaImageOutput {
    fpga_image_id: Option<String>,
    fpga_image_global_id: Option<String>,
}

fn resolve_cl_dir(cli_cl_dir: Option<PathBuf>) -> Result<PathBuf> {
    let dir = match cli_cl_dir {
        Some(path) => path,
        None => {
            let cwd = std::env::current_dir()?;
            let nested = cwd.join("cl_beethoven_top");
            if nested.is_dir() {
                nested
            } else {
                cwd
            }
        }
    };
    Ok(dir)
}

fn validate_cl_dir(cl_dir: &Path) -> Result<()> {
    require_dir(&cl_dir.join("design"))?;
    require_dir(&cl_dir.join("build").join("reports"))?;
    require_dir(&cl_dir.join("build").join("checkpoints"))?;
    Ok(())
}

fn resolve_timing_report(
    explicit: Option<&Path>,
    cl_dir: &Path,
    auto: bool,
    yes: bool,
) -> Result<PathBuf> {
    if let Some(path) = explicit {
        require_file(path)?;
        return Ok(path.to_path_buf());
    }
    let reports_dir = cl_dir.join("build").join("reports");
    let candidates = find_files_containing(&reports_dir, "post_route_timing.rpt")?;
    choose_artifact(
        "timing report",
        candidates,
        auto,
        yes,
        "Select timing report",
    )
}

fn resolve_checkpoint_tar(
    explicit: Option<&Path>,
    cl_dir: &Path,
    auto: bool,
    yes: bool,
) -> Result<PathBuf> {
    if let Some(path) = explicit {
        require_file(path)?;
        return Ok(path.to_path_buf());
    }
    let checkpoints_dir = cl_dir.join("build").join("checkpoints");
    let candidates = find_files_containing(&checkpoints_dir, ".Developer_CL.tar")?;
    choose_artifact(
        "Developer_CL.tar",
        candidates,
        auto,
        yes,
        "Select Developer_CL.tar",
    )
}

fn find_files_containing(dir: &Path, needle: &str) -> Result<Vec<PathBuf>> {
    let mut paths = Vec::new();
    for entry in WalkDir::new(dir).min_depth(1).into_iter() {
        let entry = entry.map_err(|e| anyhow::anyhow!("failed to walk {}: {e}", dir.display()))?;
        if !entry.file_type().is_file() {
            continue;
        }
        let name = entry.file_name().to_string_lossy();
        if name.contains(needle) {
            paths.push(entry.path().to_path_buf());
        }
    }
    sort_newest_first(&mut paths);
    Ok(paths)
}

fn choose_artifact(
    kind: &str,
    candidates: Vec<PathBuf>,
    auto: bool,
    yes: bool,
    prompt: &str,
) -> Result<PathBuf> {
    match candidates.len() {
        0 => Err(CliError::config(format!("no {kind} candidates found"))),
        1 => Ok(candidates[0].clone()),
        _ if auto => Ok(candidates[0].clone()),
        _ if yes => Err(CliError::config(format!(
            "multiple {kind} candidates found; pass --auto or choose explicitly"
        ))),
        _ => {
            let items: Vec<String> = candidates.iter().map(|p| p.display().to_string()).collect();
            let selection = Select::new()
                .with_prompt(prompt)
                .items(&items)
                .default(0)
                .interact()
                .map_err(|e| anyhow::anyhow!("could not read selection prompt: {e}"))?;
            Ok(candidates[selection].clone())
        }
    }
}

fn sort_newest_first(paths: &mut [PathBuf]) {
    paths.sort_by(|a, b| {
        let a_time = modified_time(a);
        let b_time = modified_time(b);
        b_time.cmp(&a_time).then_with(|| a.cmp(b))
    });
}

fn modified_time(path: &Path) -> SystemTime {
    fs::metadata(path)
        .and_then(|m| m.modified())
        .unwrap_or(UNIX_EPOCH)
}

fn validate_timing_report(path: &Path) -> Result<()> {
    let content = fs::read_to_string(path)
        .map_err(|e| anyhow::anyhow!("failed to read timing report {}: {e}", path.display()))?;
    let violated: Vec<String> = content
        .lines()
        .filter(|line| line.contains("VIOLATED"))
        .take(8)
        .map(str::to_string)
        .collect();
    if violated.is_empty() {
        return Ok(());
    }
    Err(CliError::config(format!(
        "timing report contains VIOLATED; refusing to create AFI.\nreport: {}\n{}",
        path.display(),
        violated.join("\n")
    )))
}

fn resolve_name(explicit: Option<&str>, cl_dir: &Path, yes: bool) -> Result<String> {
    let default = suggested_name(cl_dir);
    let name = match explicit {
        Some(name) => name.trim().to_string(),
        None if yes => default,
        None => Input::<String>::new()
            .with_prompt("AFI name")
            .default(default)
            .interact_text()
            .map_err(|e| anyhow::anyhow!("could not read AFI name prompt: {e}"))?,
    };
    validate_name(&name)?;
    Ok(name)
}

fn suggested_name(cl_dir: &Path) -> String {
    let stem = cl_dir
        .file_name()
        .and_then(OsStr::to_str)
        .unwrap_or("beethoven")
        .to_string();
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0);
    format!("{}-{timestamp}", sanitize_component(&stem))
}

fn validate_name(name: &str) -> Result<()> {
    if name.is_empty() {
        return Err(CliError::usage("--name cannot be empty"));
    }
    if !name
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '-'))
    {
        return Err(CliError::usage(
            "--name may only contain ASCII letters, digits, '.', '_', and '-'",
        ));
    }
    Ok(())
}

fn sanitize_component(input: &str) -> String {
    let sanitized: String = input
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '-') {
                c
            } else {
                '-'
            }
        })
        .collect();
    sanitized.trim_matches('-').to_string()
}

fn resolve_region(explicit: Option<&str>, yes: bool) -> Result<String> {
    if let Some(region) = explicit {
        let region = region.trim().to_string();
        validate_nonempty("region", &region)?;
        return Ok(region);
    }
    if let Ok(region) = std::env::var("AWS_REGION") {
        let region = region.trim().to_string();
        if !region.is_empty() {
            return Ok(region);
        }
    }
    if let Ok(region) = std::env::var("AWS_DEFAULT_REGION") {
        let region = region.trim().to_string();
        if !region.is_empty() {
            return Ok(region);
        }
    }
    if let Some(region) = aws_config_region()? {
        return Ok(region);
    }
    if yes {
        return Err(CliError::config(
            "AWS region is not configured; pass --region or set AWS_REGION",
        ));
    }
    let region = Input::<String>::new()
        .with_prompt("AWS region")
        .default("us-east-1".to_string())
        .interact_text()
        .map_err(|e| anyhow::anyhow!("could not read AWS region prompt: {e}"))?;
    validate_nonempty("region", &region)?;
    Ok(region)
}

fn aws_config_region() -> Result<Option<String>> {
    let output = Command::new("aws")
        .args(["configure", "get", "region"])
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .output()
        .map_err(|e| anyhow::anyhow!("failed to query AWS CLI region: {e}"))?;
    if !output.status.success() {
        return Ok(None);
    }
    let region = String::from_utf8_lossy(&output.stdout).trim().to_string();
    Ok((!region.is_empty()).then_some(region))
}

fn resolve_bucket(explicit: Option<&str>, name: &str, yes: bool) -> Result<String> {
    let bucket = match explicit {
        Some(bucket) => bucket.trim().to_string(),
        None => {
            let suffix = if yes {
                name.to_string()
            } else {
                Input::<String>::new()
                    .with_prompt("S3 bucket suffix")
                    .default(name.to_string())
                    .interact_text()
                    .map_err(|e| anyhow::anyhow!("could not read S3 bucket prompt: {e}"))?
            };
            let suffix = sanitize_bucket_component(&suffix);
            match aws_account_id()? {
                Some(account_id) => format!("beethoven-{suffix}-{account_id}"),
                None => format!("beethoven-{suffix}"),
            }
        }
    };
    validate_bucket(&bucket)?;
    Ok(bucket)
}

fn aws_account_id() -> Result<Option<String>> {
    let output = Command::new("aws")
        .args([
            "sts",
            "get-caller-identity",
            "--query",
            "Account",
            "--output",
            "text",
        ])
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .output()
        .map_err(|e| anyhow::anyhow!("failed to query AWS account id: {e}"))?;
    if !output.status.success() {
        return Ok(None);
    }
    let account_id = String::from_utf8_lossy(&output.stdout).trim().to_string();
    Ok((!account_id.is_empty()).then_some(account_id))
}

fn sanitize_bucket_component(input: &str) -> String {
    let sanitized: String = input
        .trim()
        .to_ascii_lowercase()
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || c == '-' {
                c
            } else {
                '-'
            }
        })
        .collect();
    sanitized.trim_matches('-').to_string()
}

fn validate_bucket(bucket: &str) -> Result<()> {
    let valid_len = (3..=63).contains(&bucket.len());
    let valid_chars = bucket
        .chars()
        .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '-' || c == '.');
    let valid_edges = bucket
        .chars()
        .next()
        .zip(bucket.chars().last())
        .map(|(first, last)| first.is_ascii_alphanumeric() && last.is_ascii_alphanumeric())
        .unwrap_or(false);
    if valid_len && valid_chars && valid_edges && !bucket.contains("..") {
        Ok(())
    } else {
        Err(CliError::usage(format!(
            "invalid S3 bucket name `{bucket}`; use 3-63 lowercase letters, digits, dots, or hyphens, starting and ending with a letter or digit"
        )))
    }
}

fn validate_nonempty(label: &str, value: &str) -> Result<()> {
    if value.trim().is_empty() {
        Err(CliError::usage(format!("{label} cannot be empty")))
    } else {
        Ok(())
    }
}

fn execute_create_fpga_image(plan: &CreateFpgaImagePlan) -> Result<()> {
    ensure_bucket(plan)?;

    ui::print_stage("Packaging", "design environment");
    create_env_tar(plan)?;

    ui::print_stage("Uploading", "Developer_CL.tar");
    let mut cp_checkpoint = Command::new("aws");
    cp_checkpoint
        .args(["s3", "cp"])
        .arg(&plan.checkpoint_tar)
        .arg(format!("s3://{}/tars/{}.tar", plan.bucket, plan.name));
    exec::run(&mut cp_checkpoint)?;

    ui::print_stage("Uploading", "design environment");
    let mut cp_env = Command::new("aws");
    cp_env
        .args(["s3", "cp"])
        .arg(&plan.env_tar)
        .arg(format!("s3://{}/env_{}.tar.gz", plan.bucket, plan.name));
    exec::run(&mut cp_env)?;

    ui::print_stage("Submitting", "create-fpga-image");
    let mut create = Command::new("aws");
    create
        .args(["ec2", "create-fpga-image"])
        .arg("--region")
        .arg(&plan.region)
        .arg("--name")
        .arg(&plan.name)
        .arg("--description")
        .arg(&plan.name)
        .arg("--input-storage-location")
        .arg(format!("Bucket={},Key=tars/{}.tar", plan.bucket, plan.name))
        .arg("--logs-storage-location")
        .arg(format!("Bucket={},Key=logs/", plan.bucket))
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    let output = create.status_output()?;
    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow::anyhow!(
            "aws ec2 create-fpga-image exited with {}\n{}",
            output.status,
            stderr.trim()
        )
        .into());
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let parsed: CreateFpgaImageOutput = serde_json::from_str(&stdout)
        .map_err(|e| anyhow::anyhow!("failed to parse create-fpga-image output: {e}\n{stdout}"))?;
    print_create_output(plan, &parsed);
    Ok(())
}

fn ensure_bucket(plan: &CreateFpgaImagePlan) -> Result<()> {
    let mut head = Command::new("aws");
    head.args(["s3api", "head-bucket"])
        .arg("--bucket")
        .arg(&plan.bucket)
        .stdout(Stdio::null())
        .stderr(Stdio::null());
    if head.status_output()?.status.success() {
        ui::print_stage("Found", &format!("S3 bucket {}", plan.bucket));
        return Ok(());
    }

    ui::print_stage("Creating", &format!("S3 bucket {}", plan.bucket));
    let mut mb = Command::new("aws");
    mb.args(["s3", "mb"])
        .arg(format!("s3://{}", plan.bucket))
        .arg("--region")
        .arg(&plan.region);
    exec::run(&mut mb)?;
    Ok(())
}

trait CommandOutputExt {
    fn status_output(&mut self) -> std::io::Result<std::process::Output>;
}

impl CommandOutputExt for Command {
    fn status_output(&mut self) -> std::io::Result<std::process::Output> {
        ui::print_exec(self);
        self.output()
    }
}

fn create_env_tar(plan: &CreateFpgaImagePlan) -> Result<()> {
    let design_dir = plan.cl_dir.join("design");
    let entries = fs::read_dir(&design_dir)
        .map_err(|e| anyhow::anyhow!("failed to read {}: {e}", design_dir.display()))?;
    let mut names = Vec::new();
    for entry in entries {
        let entry = entry?;
        names.push(entry.file_name());
    }
    if names.is_empty() {
        return Err(CliError::config(format!(
            "design directory is empty: {}",
            design_dir.display()
        )));
    }
    names.sort();

    let mut tar = Command::new("tar");
    tar.arg("-C")
        .arg(&design_dir)
        .arg("-czf")
        .arg(&plan.env_tar)
        .args(names);
    exec::run(&mut tar)?;
    Ok(())
}

fn print_create_output(plan: &CreateFpgaImagePlan, output: &CreateFpgaImageOutput) {
    println!();
    println!("FPGA image creation submitted");
    println!();
    println!(
        "FpgaImageId:        {}",
        output.fpga_image_id.as_deref().unwrap_or("<missing>")
    );
    println!(
        "FpgaImageGlobalId:  {}",
        output
            .fpga_image_global_id
            .as_deref()
            .unwrap_or("<missing>")
    );
    println!();
    println!("Logs:");
    println!("s3://{}/logs/", plan.bucket);
}

fn print_dry_run(plan: &CreateFpgaImagePlan) {
    println!("Dry run: create-fpga-image plan");
    println!();
    println!("CL directory:              {}", plan.cl_dir.display());
    println!(
        "selected timing report:    {}",
        plan.timing_report.display()
    );
    println!(
        "selected checkpoint tar:   {}",
        plan.checkpoint_tar.display()
    );
    println!("AFI name:                  {}", plan.name);
    println!("AWS region:                {}", plan.region);
    println!("S3 bucket:                 {}", plan.bucket);
    println!(
        "checkpoint destination:    s3://{}/tars/{}.tar",
        plan.bucket, plan.name
    );
    println!(
        "environment destination:   s3://{}/env_{}.tar.gz",
        plan.bucket, plan.name
    );
    println!();
    println!("Commands:");
    println!(
        "aws s3api head-bucket --bucket {}",
        shell_quote(&plan.bucket)
    );
    println!(
        "aws s3 mb s3://{} --region {}",
        shell_quote(&plan.bucket),
        shell_quote(&plan.region)
    );
    println!(
        "tar -C {} -czf {} <all entries in design/>",
        shell_quote_path(&plan.cl_dir.join("design")),
        shell_quote_path(&plan.env_tar)
    );
    println!(
        "aws s3 cp {} s3://{}/tars/{}.tar",
        shell_quote_path(&plan.checkpoint_tar),
        shell_quote(&plan.bucket),
        shell_quote(&plan.name)
    );
    println!(
        "aws s3 cp {} s3://{}/env_{}.tar.gz",
        shell_quote_path(&plan.env_tar),
        shell_quote(&plan.bucket),
        shell_quote(&plan.name)
    );
    println!(
        "aws ec2 create-fpga-image --region {} --name {} --description {} --input-storage-location {} --logs-storage-location {}",
        shell_quote(&plan.region),
        shell_quote(&plan.name),
        shell_quote(&plan.name),
        shell_quote(&format!("Bucket={},Key=tars/{}.tar", plan.bucket, plan.name)),
        shell_quote(&format!("Bucket={},Key=logs/", plan.bucket)),
    );
}

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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bucket_component_is_s3_safe() {
        assert_eq!(sanitize_bucket_component("Test Key_01"), "test-key-01");
        assert_eq!(sanitize_bucket_component("--A--"), "a");
    }

    #[test]
    fn bucket_validation_rejects_uppercase() {
        assert!(validate_bucket("beethoven-test-123").is_ok());
        assert!(validate_bucket("Beethoven-test").is_err());
        assert!(validate_bucket("-beethoven-test").is_err());
    }

    #[test]
    fn name_validation_allows_key_friendly_chars() {
        assert!(validate_name("test-key_01.afi").is_ok());
        assert!(validate_name("bad/name").is_err());
    }
}
