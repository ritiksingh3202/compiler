//! Expression AST nodes.

use tiny_lexer::Span;

/// Unary operators.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnaryOp {
    /// `-expr`
    Neg,
    /// `!expr`
    Not,
}

/// Binary operators (arithmetic, comparison, logical).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
}

/// A Tiny expression.
#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    IntLit {
        value: i64,
        span: Span,
    },
    StringLit {
        value: String,
        span: Span,
    },
    BoolLit {
        value: bool,
        span: Span,
    },
    Ident {
        name: String,
        span: Span,
    },
    Unary {
        op: UnaryOp,
        expr: Box<Expr>,
        span: Span,
    },
    Binary {
        op: BinaryOp,
        left: Box<Expr>,
        right: Box<Expr>,
        span: Span,
    },
    /// `name(args…)` — callee is a bare identifier for now.
    Call {
        callee: String,
        args: Vec<Expr>,
        span: Span,
    },
    Assign {
        name: String,
        value: Box<Expr>,
        span: Span,
    },
}

impl Expr {
    pub fn span(&self) -> Span {
        match self {
            Self::IntLit { span, .. }
            | Self::StringLit { span, .. }
            | Self::BoolLit { span, .. }
            | Self::Ident { span, .. }
            | Self::Unary { span, .. }
            | Self::Binary { span, .. }
            | Self::Call { span, .. }
            | Self::Assign { span, .. } => *span,
        }
    }
}
