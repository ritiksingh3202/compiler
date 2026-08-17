//! Tree-walking interpreter for Tiny (reference semantics).

mod error;
mod eval;
mod value;

pub use error::RuntimeError;
pub use eval::{run, run_to_string};
pub use value::Value;
