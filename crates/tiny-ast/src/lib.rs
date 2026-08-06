//! Pure AST data types for the Tiny programming language.
//!
//! No parsing logic lives here — see `tiny-parser`.

mod expr;
mod stmt;
mod ty;

pub use expr::{BinaryOp, Expr, UnaryOp};
pub use stmt::{Block, FnDecl, Param, Program, Stmt};
pub use ty::Type;

pub use tiny_lexer::{Position, Span};
