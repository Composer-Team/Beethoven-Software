//! Runtime daemon helpers — flock probe + spawn / launch-command /
//! exit-reporting lifecycle. Used by `runtime run` and (in phase 7)
//! by `sim` / `run`.

pub mod lifecycle;
pub mod probe;
