//! Foundational utilities — path resolution, process exec.
//!
//! These have no domain knowledge; they're the things every other
//! module reaches for when it needs to talk to the filesystem,
//! environment, or another process.

pub mod env;
pub mod exec;
