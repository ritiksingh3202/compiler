//! Parse errors with source location and a caret pointer.

use thiserror::Error;
use tiny_lexer::{LexError, Span, TokenKind};

/// A syntax error produced by the parser.
#[derive(Debug, Error, Clone, PartialEq, Eq)]
#[error("{formatted}")]
pub struct ParseError {
    pub message: String,
    pub span: Span,
    pub expected: Option<String>,
    pub found: Option<String>,
    /// Multi-line diagnostic with a caret under the offending token.
    pub formatted: String,
}

impl ParseError {
    pub fn new(
        message: impl Into<String>,
        span: Span,
        source: &str,
        expected: Option<String>,
        found: Option<String>,
    ) -> Self {
        let message = message.into();
        let detail = match (&expected, &found) {
            (Some(e), Some(f)) => format!("{message} (expected {e}, found {f})"),
            (Some(e), None) => format!("{message} (expected {e})"),
            (None, Some(f)) => format!("{message} (found {f})"),
            (None, None) => message.clone(),
        };
        let formatted = format_diagnostic(&detail, span, source);
        Self {
            message,
            span,
            expected,
            found,
            formatted,
        }
    }

    pub fn unexpected(
        span: Span,
        source: &str,
        expected: impl Into<String>,
        found: &TokenKind,
    ) -> Self {
        let expected = expected.into();
        let found_s = found.to_string();
        Self::new(
            format!("expected {expected}, found {found_s}"),
            span,
            source,
            Some(expected),
            Some(found_s),
        )
    }

    pub fn from_lex(err: LexError) -> Self {
        Self {
            message: err.message,
            span: err.span,
            expected: None,
            found: None,
            formatted: err.formatted,
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
    let caret_len = if span.end.line == span.start.line {
        (span.end.column.saturating_sub(span.start.column)).max(1) as usize
    } else {
        1
    };
    let caret = format!("{}{}", " ".repeat(caret_col), "^".repeat(caret_len));

    format!(
        "error: {message}\n\
         {pad} --> {line_no}:{col}\n\
         {pad}  |\n\
         {gutter}  | {line_text}\n\
         {pad}  | {caret}"
    )
}
