//! Interpreter errors.

use thiserror::Error;
use tiny_lexer::Span;

#[derive(Debug, Error, Clone, PartialEq, Eq)]
#[error("{formatted}")]
pub struct RuntimeError {
    pub message: String,
    pub span: Span,
    pub formatted: String,
}

impl RuntimeError {
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
    let caret = format!("{}{}", " ".repeat(col.saturating_sub(1) as usize), "^");
    format!(
        "error: {message}\n{pad} --> {line_no}:{col}\n{pad}  |\n{gutter}  | {line_text}\n{pad}  | {caret}"
    )
}
