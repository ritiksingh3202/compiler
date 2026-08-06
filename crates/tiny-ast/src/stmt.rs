//! Statement and declaration AST nodes.

use crate::expr::Expr;
use crate::ty::Type;
use tiny_lexer::Span;

/// A braced statement list.
#[derive(Debug, Clone, PartialEq)]
pub struct Block {
    pub statements: Vec<Stmt>,
    pub span: Span,
}

/// Function parameter `name: type`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Param {
    pub name: String,
    pub ty: Type,
    pub span: Span,
}

/// A function declaration.
#[derive(Debug, Clone, PartialEq)]
pub struct FnDecl {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Type,
    pub body: Block,
    pub span: Span,
}

/// A Tiny statement.
#[derive(Debug, Clone, PartialEq)]
pub enum Stmt {
    Let {
        name: String,
        ty: Option<Type>,
        init: Expr,
        span: Span,
    },
    /// Expression statement; requires a trailing `;`.
    Expr(Expr),
    Return {
        value: Option<Expr>,
        span: Span,
    },
    If {
        cond: Expr,
        then_block: Block,
        else_block: Option<Block>,
        span: Span,
    },
    While {
        cond: Expr,
        body: Block,
        span: Span,
    },
    /// Standalone `{ … }` block used as a statement.
    Block(Block),
}

impl Stmt {
    pub fn span(&self) -> Span {
        match self {
            Self::Let { span, .. }
            | Self::Return { span, .. }
            | Self::If { span, .. }
            | Self::While { span, .. } => *span,
            Self::Expr(expr) => expr.span(),
            Self::Block(block) => block.span,
        }
    }
}

/// Top-level program: function declarations plus bare statements.
#[derive(Debug, Clone, PartialEq)]
pub struct Program {
    pub functions: Vec<FnDecl>,
    pub statements: Vec<Stmt>,
    pub span: Span,
}
