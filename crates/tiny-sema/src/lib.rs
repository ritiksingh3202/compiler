//! Semantic analysis for Tiny: scopes, symbol tables, and type checking.

mod error;
mod symbols;
mod typeck;

pub use error::{SemaError, SemaErrors};
pub use symbols::{FnSig, ScopeStack};
pub use typeck::check;
