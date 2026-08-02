//! Token kinds and token values for Tiny.

use crate::span::Span;

/// A single lexed token with its source span and original lexeme.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Token {
    pub kind: TokenKind,
    pub span: Span,
    pub lexeme: String,
}

impl Token {
    pub fn new(kind: TokenKind, span: Span, lexeme: impl Into<String>) -> Self {
        Self {
            kind,
            span,
            lexeme: lexeme.into(),
        }
    }
}

/// Classification of a Tiny token.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TokenKind {
    // Keywords
    Let,
    Fn,
    If,
    Else,
    While,
    Return,
    True,
    False,
    Int,
    Bool,
    String,

    // Literals & identifiers
    Ident(String),
    IntLit(i64),
    StringLit(String),

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    EqEq,
    BangEq,
    Lt,
    LtEq,
    Gt,
    GtEq,
    Eq,
    Bang,
    AmpAmp,
    PipePipe,
    Arrow,

    // Punctuation
    LParen,
    RParen,
    LBrace,
    RBrace,
    Comma,
    Colon,
    Semi,

    Eof,
}

impl TokenKind {
    /// Map an identifier spelling to a keyword, if it is one.
    pub fn keyword(ident: &str) -> Option<Self> {
        Some(match ident {
            "let" => Self::Let,
            "fn" => Self::Fn,
            "if" => Self::If,
            "else" => Self::Else,
            "while" => Self::While,
            "return" => Self::Return,
            "true" => Self::True,
            "false" => Self::False,
            "int" => Self::Int,
            "bool" => Self::Bool,
            "string" => Self::String,
            _ => return None,
        })
    }

    pub fn is_keyword(&self) -> bool {
        matches!(
            self,
            Self::Let
                | Self::Fn
                | Self::If
                | Self::Else
                | Self::While
                | Self::Return
                | Self::True
                | Self::False
                | Self::Int
                | Self::Bool
                | Self::String
        )
    }
}

impl std::fmt::Display for TokenKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            Self::Let => "let",
            Self::Fn => "fn",
            Self::If => "if",
            Self::Else => "else",
            Self::While => "while",
            Self::Return => "return",
            Self::True => "true",
            Self::False => "false",
            Self::Int => "int",
            Self::Bool => "bool",
            Self::String => "string",
            Self::Ident(name) => return write!(f, "identifier `{name}`"),
            Self::IntLit(n) => return write!(f, "integer `{n}`"),
            Self::StringLit(s) => return write!(f, "string {s:?}"),
            Self::Plus => "+",
            Self::Minus => "-",
            Self::Star => "*",
            Self::Slash => "/",
            Self::Percent => "%",
            Self::EqEq => "==",
            Self::BangEq => "!=",
            Self::Lt => "<",
            Self::LtEq => "<=",
            Self::Gt => ">",
            Self::GtEq => ">=",
            Self::Eq => "=",
            Self::Bang => "!",
            Self::AmpAmp => "&&",
            Self::PipePipe => "||",
            Self::Arrow => "->",
            Self::LParen => "(",
            Self::RParen => ")",
            Self::LBrace => "{",
            Self::RBrace => "}",
            Self::Comma => ",",
            Self::Colon => ":",
            Self::Semi => ";",
            Self::Eof => "<eof>",
        };
        write!(f, "`{s}`")
    }
}
