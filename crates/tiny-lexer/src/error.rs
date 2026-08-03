//! Lexical errors with source location and a caret pointer.

use crate::span::{Position, Span};
use thiserror::Error;

/// A lexical analysis error.
#[derive(Debug, Error, Clone, PartialEq, Eq)]
#[error("{formatted}")]
pub struct LexError {
    pub message: String,
    pub span: Span,
    /// Multi-line diagnostic: location, source line, and `^` under the bad char.
    pub formatted: String,
}

impl LexError {
    pub fn new(message: impl Into<String>, span: Span, source: &str) -> Self {
        let message = message.into();
        let formatted = format_diagnostic(&message, span, source);
        Self {
            message,
            span,
            formatted,
        }
    }
}

fn format_diagnostic(message: &str, span: Span, source: &str) -> String {
    let line_no = span.start.line;
    let col = span.start.column;
    let line_text = source
        .lines()
        .nth(line_no.saturating_sub(1) as usize)
        .unwrap_or("");

    let gutter = format!("{line_no}");
    let pad = " ".repeat(gutter.len());
    let caret_col = col.saturating_sub(1) as usize;
    let caret = format!("{}{}", " ".repeat(caret_col), "^");

    format!(
        "error: {message}\n\
         {pad} --> {line_no}:{col}\n\
         {pad}  |\n\
         {gutter}  | {line_text}\n\
         {pad}  | {caret}"
    )
}

/// Convenience constructor when only a single position is known.
pub fn error_at(message: impl Into<String>, pos: Position, source: &str) -> LexError {
    LexError::new(message, Span::empty(pos), source)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn diagnostic_points_at_column() {
        let src = "let x = @;\n";
        let err = error_at(
            "unexpected character `@`",
            Position::new(1, 9),
            src,
        );
        assert!(err.formatted.contains("--> 1:9"));
        assert!(err.formatted.contains("let x = @;"));
        assert!(err.formatted.contains("        ^"));
    }
}
