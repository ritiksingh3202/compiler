//! Semantic analysis errors with caret diagnostics.

use thiserror::Error;
use tiny_lexer::Span;

/// A single semantic error.
#[derive(Debug, Error, Clone, PartialEq, Eq)]
#[error("{formatted}")]
pub struct SemaError {
    pub message: String,
    pub span: Span,
    pub formatted: String,
}

impl SemaError {
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

/// One or more semantic errors from a check pass.
#[derive(Debug, Error, Clone, PartialEq, Eq)]
#[error("{}", .errors.iter().map(|e| e.formatted.as_str()).collect::<Vec<_>>().join("\n\n"))]
pub struct SemaErrors {
    pub errors: Vec<SemaError>,
}

impl SemaErrors {
    pub fn from_vec(errors: Vec<SemaError>) -> Result<(), Self> {
        if errors.is_empty() {
            Ok(())
        } else {
            Err(Self { errors })
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
