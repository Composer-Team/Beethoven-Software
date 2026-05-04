//! Wrappers around external programs (git, cmake). Each submodule
//! owns the argv construction and error mapping for its tool; nothing
//! else in the crate should `Command::new("git")` or `cmake` directly.

pub mod cmake;
pub mod git;
pub mod sbt;
pub mod vivado;
