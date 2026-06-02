//! CLI error type and exit-code mapping. Exit codes follow the table
//! in `cli/docs/README.md`.

use thiserror::Error;

pub type Result<T> = std::result::Result<T, CliError>;

#[derive(Error, Debug)]
pub enum CliError {
    /// Bad CLI usage — wrong flags, missing required args, etc.
    #[error("{0}")]
    Usage(String),

    /// Configuration error: malformed Beethoven.toml, missing prefix,
    /// not in a project, etc.
    #[error("{0}")]
    Config(String),

    /// A required external tool is missing from PATH.
    #[error("required tool '{name}' not found on PATH")]
    MissingTool { name: String, hint: Option<String> },

    /// Anything else: build failures, IO errors, network errors.
    #[error(transparent)]
    Other(#[from] anyhow::Error),
}

impl CliError {
    /// Exit code returned to the shell.
    pub fn exit_code(&self) -> i32 {
        match self {
            Self::Usage(_) => 2,
            Self::Config(_) => 64,
            Self::MissingTool { .. } => 127,
            Self::Other(_) => 1,
        }
    }

    /// Optional hint shown alongside the error.
    pub fn hint(&self) -> Option<&str> {
        match self {
            Self::MissingTool { hint, .. } => hint.as_deref(),
            _ => None,
        }
    }

    pub fn usage(msg: impl Into<String>) -> Self {
        Self::Usage(msg.into())
    }

    pub fn config(msg: impl Into<String>) -> Self {
        Self::Config(msg.into())
    }

    pub fn missing_tool(name: impl Into<String>, hint: Option<String>) -> Self {
        Self::MissingTool {
            name: name.into(),
            hint,
        }
    }
}

impl From<std::io::Error> for CliError {
    fn from(e: std::io::Error) -> Self {
        Self::Other(e.into())
    }
}
