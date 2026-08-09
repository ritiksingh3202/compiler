//! Recursive-descent parser with Pratt expressions for Tiny.

mod error;
mod parser;

pub use error::ParseError;
pub use parser::parse;
