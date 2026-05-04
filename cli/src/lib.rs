//! Library facade exposing the CLI's modules. `main.rs` is a thin
//! shim over this crate; integration tests link against this lib.

pub mod cli;
pub mod commands;
pub mod core;
pub mod error;
pub mod runtime;
pub mod state;
pub mod template;
pub mod tools;
pub mod ui;
