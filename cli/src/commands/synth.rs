use crate::error::Result;
use crate::ui;

pub fn run() -> Result<()> {
    ui::print_note(
        "`synth` is reserved for future Vivado integration — see cli/docs/synth.md.",
    );
    Ok(())
}
