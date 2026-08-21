//! Intermediate representation and AST lowering for Tiny.

mod ir;
mod lower;
mod opt;

pub use ir::{CmpOp, Instr, IrFunction, Label, Module, PrintKind, Reg};
pub use lower::lower;
pub use opt::optimize;
