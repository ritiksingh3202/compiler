//! Hand-written recursive-descent parser with Pratt expressions.

use crate::error::ParseError;
use tiny_ast::{
    BinaryOp, Block, Expr, FnDecl, Param, Program, Stmt, Type, UnaryOp,
};
use tiny_lexer::{tokenize, Position, Span, Token, TokenKind};

/// Parse Tiny source into an AST [`Program`].
pub fn parse(source: &str) -> Result<Program, ParseError> {
    let tokens = tokenize(source).map_err(ParseError::from_lex)?;
    if tokens.is_empty() {
        return Err(ParseError::new(
            "empty token stream",
            Span::empty(Position::start()),
            source,
            None,
            None,
        ));
    }
    Parser::new(&tokens, source).parse_program()
}

struct Parser<'a> {
    tokens: &'a [Token],
    pos: usize,
    source: &'a str,
}

impl<'a> Parser<'a> {
    fn new(tokens: &'a [Token], source: &'a str) -> Self {
        Self {
            tokens,
            pos: 0,
            source,
        }
    }

    fn parse_program(&mut self) -> Result<Program, ParseError> {
        let start = self.peek().span.start;
        let mut functions = Vec::new();
        let mut statements = Vec::new();

        while !self.is_at_end() {
            if self.check(&TokenKind::Fn) {
                functions.push(self.parse_fn_decl()?);
            } else {
                statements.push(self.parse_stmt()?);
            }
        }

        let end = self.previous_end(start);
        Ok(Program {
            functions,
            statements,
            span: Span::new(start, end),
        })
    }

    fn parse_fn_decl(&mut self) -> Result<FnDecl, ParseError> {
        let start = self.peek().span.start;
        self.expect_kind(&TokenKind::Fn, "`fn`")?;
        let (name, _) = self.expect_ident("`function name`")?;
        self.expect_kind(&TokenKind::LParen, "`(`")?;

        let mut params = Vec::new();
        if !self.check(&TokenKind::RParen) {
            loop {
                params.push(self.parse_param()?);
                if !self.match_kind(&TokenKind::Comma) {
                    break;
                }
            }
        }
        self.expect_kind(&TokenKind::RParen, "`)`")?;
        // Optional `-> type`; omitted means Void (Milestone 3).
        let return_type = if self.match_kind(&TokenKind::Arrow) {
            self.parse_type()?
        } else {
            Type::Void
        };
        let body = self.parse_block()?;
        let end = body.span.end;

        Ok(FnDecl {
            name,
            params,
            return_type,
            body,
            span: Span::new(start, end),
        })
    }

    fn parse_param(&mut self) -> Result<Param, ParseError> {
        let (name, name_span) = self.expect_ident("`parameter name`")?;
        self.expect_kind(&TokenKind::Colon, "`:`")?;
        let ty = self.parse_type()?;
        let end = self.previous_end(name_span.start);
        Ok(Param {
            name,
            ty,
            span: Span::new(name_span.start, end),
        })
    }

    fn parse_type(&mut self) -> Result<Type, ParseError> {
        let tok = self.peek().clone();
        let ty = match tok.kind {
            TokenKind::Int => Type::Int,
            TokenKind::Bool => Type::Bool,
            TokenKind::String => Type::String,
            ref other => {
                return Err(ParseError::unexpected(
                    tok.span,
                    self.source,
                    "type (`int`, `bool`, or `string`)",
                    other,
                ));
            }
        };
        self.advance();
        Ok(ty)
    }

    fn parse_stmt(&mut self) -> Result<Stmt, ParseError> {
        match &self.peek().kind {
            TokenKind::Let => self.parse_let(),
            TokenKind::Return => self.parse_return(),
            TokenKind::If => self.parse_if(),
            TokenKind::While => self.parse_while(),
            TokenKind::LBrace => Ok(Stmt::Block(self.parse_block()?)),
            _ => self.parse_expr_stmt(),
        }
    }

    fn parse_let(&mut self) -> Result<Stmt, ParseError> {
        let start = self.peek().span.start;
        self.expect_kind(&TokenKind::Let, "`let`")?;
        let (name, _) = self.expect_ident("`variable name`")?;

        let ty = if self.match_kind(&TokenKind::Colon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        self.expect_kind(&TokenKind::Eq, "`=`")?;
        let init = self.parse_expr(0)?;
        self.expect_kind(&TokenKind::Semi, "`;`")?;
        let end = self.previous_end(start);

        Ok(Stmt::Let {
            name,
            ty,
            init,
            span: Span::new(start, end),
        })
    }

    fn parse_return(&mut self) -> Result<Stmt, ParseError> {
        let start = self.peek().span.start;
        self.expect_kind(&TokenKind::Return, "`return`")?;

        let value = if self.check(&TokenKind::Semi) {
            None
        } else {
            Some(self.parse_expr(0)?)
        };
        self.expect_kind(&TokenKind::Semi, "`;`")?;
        let end = self.previous_end(start);

        Ok(Stmt::Return {
            value,
            span: Span::new(start, end),
        })
    }

    fn parse_if(&mut self) -> Result<Stmt, ParseError> {
        let start = self.peek().span.start;
        self.expect_kind(&TokenKind::If, "`if`")?;
        let cond = self.parse_expr(0)?;
        let then_block = self.parse_block()?;

        let else_block = if self.match_kind(&TokenKind::Else) {
            if self.check(&TokenKind::If) {
                let nested = self.parse_if()?;
                let span = nested.span();
                Some(Block {
                    statements: vec![nested],
                    span,
                })
            } else {
                Some(self.parse_block()?)
            }
        } else {
            None
        };

        let end = match &else_block {
            Some(b) => b.span.end,
            None => then_block.span.end,
        };

        Ok(Stmt::If {
            cond,
            then_block,
            else_block,
            span: Span::new(start, end),
        })
    }

    fn parse_while(&mut self) -> Result<Stmt, ParseError> {
        let start = self.peek().span.start;
        self.expect_kind(&TokenKind::While, "`while`")?;
        let cond = self.parse_expr(0)?;
        let body = self.parse_block()?;
        let end = body.span.end;
        Ok(Stmt::While {
            cond,
            body,
            span: Span::new(start, end),
        })
    }

    fn parse_block(&mut self) -> Result<Block, ParseError> {
        let start = self.peek().span.start;
        self.expect_kind(&TokenKind::LBrace, "`{`")?;
        let mut statements = Vec::new();
        while !self.check(&TokenKind::RBrace) && !self.is_at_end() {
            statements.push(self.parse_stmt()?);
        }
        self.expect_kind(&TokenKind::RBrace, "`}`")?;
        let end = self.previous_end(start);
        Ok(Block {
            statements,
            span: Span::new(start, end),
        })
    }

    fn parse_expr_stmt(&mut self) -> Result<Stmt, ParseError> {
        let expr = self.parse_expr(0)?;
        self.expect_kind(&TokenKind::Semi, "`;`")?;
        Ok(Stmt::Expr(expr))
    }

    /// Pratt / precedence-climbing expression parser.
    fn parse_expr(&mut self, min_bp: u8) -> Result<Expr, ParseError> {
        let mut left = self.parse_prefix()?;

        loop {
            let Some((l_bp, r_bp, op)) = infix_binding(&self.peek().kind) else {
                break;
            };
            if l_bp < min_bp {
                break;
            }
            self.advance(); // consume operator

            if matches!(op, InfixOp::Assign) {
                let (name, name_span) = match left {
                    Expr::Ident { name, span } => (name, span),
                    other => {
                        return Err(ParseError::new(
                            "invalid assignment target",
                            other.span(),
                            self.source,
                            Some("identifier".into()),
                            None,
                        ));
                    }
                };
                let value = self.parse_expr(r_bp)?;
                left = Expr::Assign {
                    name,
                    span: Span::new(name_span.start, value.span().end),
                    value: Box::new(value),
                };
                continue;
            }

            let right = self.parse_expr(r_bp)?;
            let span = Span::new(left.span().start, right.span().end);
            let InfixOp::Binary(bin) = op else {
                // Assign handled above
                continue;
            };
            left = Expr::Binary {
                op: bin,
                left: Box::new(left),
                right: Box::new(right),
                span,
            };
        }

        Ok(left)
    }

    fn parse_prefix(&mut self) -> Result<Expr, ParseError> {
        let tok = self.peek().clone();
        match tok.kind {
            TokenKind::Minus => {
                self.advance();
                let expr = self.parse_expr(PREFIX_BP)?;
                Ok(Expr::Unary {
                    op: UnaryOp::Neg,
                    span: Span::new(tok.span.start, expr.span().end),
                    expr: Box::new(expr),
                })
            }
            TokenKind::Bang => {
                self.advance();
                let expr = self.parse_expr(PREFIX_BP)?;
                Ok(Expr::Unary {
                    op: UnaryOp::Not,
                    span: Span::new(tok.span.start, expr.span().end),
                    expr: Box::new(expr),
                })
            }
            _ => self.parse_call_or_primary(),
        }
    }

    fn parse_call_or_primary(&mut self) -> Result<Expr, ParseError> {
        let expr = self.parse_primary()?;

        if !self.check(&TokenKind::LParen) {
            return Ok(expr);
        }

        let Expr::Ident {
            name: callee,
            span: name_span,
        } = expr
        else {
            return Err(ParseError::new(
                "only identifiers can be called",
                expr.span(),
                self.source,
                None,
                None,
            ));
        };

        self.advance(); // (
        let mut args = Vec::new();
        if !self.check(&TokenKind::RParen) {
            loop {
                args.push(self.parse_expr(0)?);
                if !self.match_kind(&TokenKind::Comma) {
                    break;
                }
            }
        }
        let rparen = self.expect_kind(&TokenKind::RParen, "`)`")?;
        Ok(Expr::Call {
            callee,
            args,
            span: Span::new(name_span.start, rparen.span.end),
        })
    }

    fn parse_primary(&mut self) -> Result<Expr, ParseError> {
        let tok = self.peek().clone();
        match tok.kind {
            TokenKind::IntLit(value) => {
                self.advance();
                Ok(Expr::IntLit {
                    value,
                    span: tok.span,
                })
            }
            TokenKind::StringLit(value) => {
                self.advance();
                Ok(Expr::StringLit {
                    value,
                    span: tok.span,
                })
            }
            TokenKind::True => {
                self.advance();
                Ok(Expr::BoolLit {
                    value: true,
                    span: tok.span,
                })
            }
            TokenKind::False => {
                self.advance();
                Ok(Expr::BoolLit {
                    value: false,
                    span: tok.span,
                })
            }
            TokenKind::Ident(name) => {
                self.advance();
                Ok(Expr::Ident {
                    name,
                    span: tok.span,
                })
            }
            TokenKind::LParen => {
                self.advance();
                let expr = self.parse_expr(0)?;
                self.expect_kind(&TokenKind::RParen, "`)`")?;
                Ok(expr)
            }
            ref other => Err(ParseError::unexpected(
                tok.span,
                self.source,
                "expression",
                other,
            )),
        }
    }

    fn peek(&self) -> &Token {
        match self.tokens.get(self.pos) {
            Some(t) => t,
            None => &self.tokens[self.tokens.len() - 1],
        }
    }

    fn is_at_end(&self) -> bool {
        matches!(self.peek().kind, TokenKind::Eof)
    }

    fn same_kind(a: &TokenKind, b: &TokenKind) -> bool {
        std::mem::discriminant(a) == std::mem::discriminant(b)
    }

    fn check(&self, kind: &TokenKind) -> bool {
        Self::same_kind(&self.peek().kind, kind)
    }

    fn advance(&mut self) -> Token {
        let tok = self.peek().clone();
        if !self.is_at_end() {
            self.pos += 1;
        }
        tok
    }

    fn match_kind(&mut self, kind: &TokenKind) -> bool {
        if self.check(kind) {
            self.advance();
            true
        } else {
            false
        }
    }

    fn expect_kind(&mut self, kind: &TokenKind, expected: &str) -> Result<Token, ParseError> {
        if self.check(kind) {
            Ok(self.advance())
        } else {
            let tok = self.peek();
            Err(ParseError::unexpected(
                tok.span,
                self.source,
                expected,
                &tok.kind,
            ))
        }
    }

    fn expect_ident(&mut self, expected: &str) -> Result<(String, Span), ParseError> {
        let tok = self.peek().clone();
        match tok.kind {
            TokenKind::Ident(name) => {
                self.advance();
                Ok((name, tok.span))
            }
            ref other => Err(ParseError::unexpected(
                tok.span,
                self.source,
                expected,
                other,
            )),
        }
    }

    fn previous_end(&self, fallback: Position) -> Position {
        if self.pos == 0 {
            fallback
        } else {
            self.tokens[self.pos - 1].span.end
        }
    }
}

#[derive(Clone, Copy)]
enum InfixOp {
    Assign,
    Binary(BinaryOp),
}

const PREFIX_BP: u8 = 8;

/// Returns `(left_bp, right_bp, op)` for infix operators.
fn infix_binding(kind: &TokenKind) -> Option<(u8, u8, InfixOp)> {
    // left-assoc: (bp, bp+1); right-assoc assign: (1, 1)
    match kind {
        TokenKind::Eq => Some((1, 1, InfixOp::Assign)),
        TokenKind::PipePipe => Some((2, 3, InfixOp::Binary(BinaryOp::Or))),
        TokenKind::AmpAmp => Some((3, 4, InfixOp::Binary(BinaryOp::And))),
        TokenKind::EqEq => Some((4, 5, InfixOp::Binary(BinaryOp::Eq))),
        TokenKind::BangEq => Some((4, 5, InfixOp::Binary(BinaryOp::Ne))),
        TokenKind::Lt => Some((5, 6, InfixOp::Binary(BinaryOp::Lt))),
        TokenKind::LtEq => Some((5, 6, InfixOp::Binary(BinaryOp::Le))),
        TokenKind::Gt => Some((5, 6, InfixOp::Binary(BinaryOp::Gt))),
        TokenKind::GtEq => Some((5, 6, InfixOp::Binary(BinaryOp::Ge))),
        TokenKind::Plus => Some((6, 7, InfixOp::Binary(BinaryOp::Add))),
        TokenKind::Minus => Some((6, 7, InfixOp::Binary(BinaryOp::Sub))),
        TokenKind::Star => Some((7, 8, InfixOp::Binary(BinaryOp::Mul))),
        TokenKind::Slash => Some((7, 8, InfixOp::Binary(BinaryOp::Div))),
        TokenKind::Percent => Some((7, 8, InfixOp::Binary(BinaryOp::Rem))),
        _ => None,
    }
}
