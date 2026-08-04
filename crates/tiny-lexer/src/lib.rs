//! Hand-written lexer for the Tiny programming language.
//!
//! Converts source text into a stream of [`Token`]s with accurate
//! [`Span`] information for diagnostics.

mod error;
mod lexer;
mod span;
mod token;

pub use error::LexError;
pub use lexer::{tokenize, Lexer};
pub use span::{Position, Span};
pub use token::{Token, TokenKind};
