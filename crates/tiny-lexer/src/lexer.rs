//! Hand-written lexer: source text → tokens.

use crate::error::{error_at, LexError};
use crate::span::{Position, Span};
use crate::token::{Token, TokenKind};

/// Lex `source` into a token stream (including a trailing [`TokenKind::Eof`]).
pub fn tokenize(source: &str) -> Result<Vec<Token>, LexError> {
    Lexer::new(source).tokenize()
}

/// Streaming lexer over a Tiny source string.
pub struct Lexer<'a> {
    source: &'a str,
    chars: Vec<char>,
    index: usize,
    line: u32,
    column: u32,
}

impl<'a> Lexer<'a> {
    pub fn new(source: &'a str) -> Self {
        Self {
            source,
            chars: source.chars().collect(),
            index: 0,
            line: 1,
            column: 1,
        }
    }

    pub fn tokenize(mut self) -> Result<Vec<Token>, LexError> {
        let mut tokens = Vec::new();
        loop {
            let token = self.next_token()?;
            let is_eof = token.kind == TokenKind::Eof;
            tokens.push(token);
            if is_eof {
                break;
            }
        }
        Ok(tokens)
    }

    fn next_token(&mut self) -> Result<Token, LexError> {
        self.skip_trivia()?;
        let start = self.position();

        let Some(ch) = self.peek() else {
            return Ok(Token::new(TokenKind::Eof, Span::empty(start), ""));
        };

        match ch {
            '(' => Ok(self.simple(TokenKind::LParen, "(")),
            ')' => Ok(self.simple(TokenKind::RParen, ")")),
            '{' => Ok(self.simple(TokenKind::LBrace, "{")),
            '}' => Ok(self.simple(TokenKind::RBrace, "}")),
            ',' => Ok(self.simple(TokenKind::Comma, ",")),
            ':' => Ok(self.simple(TokenKind::Colon, ":")),
            ';' => Ok(self.simple(TokenKind::Semi, ";")),
            '+' => Ok(self.simple(TokenKind::Plus, "+")),
            '*' => Ok(self.simple(TokenKind::Star, "*")),
            '%' => Ok(self.simple(TokenKind::Percent, "%")),
            '-' => {
                self.bump();
                if self.peek() == Some('>') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::Arrow,
                        Span::new(start, self.position()),
                        "->",
                    ))
                } else {
                    Ok(Token::new(
                        TokenKind::Minus,
                        Span::new(start, self.position()),
                        "-",
                    ))
                }
            }
            '/' => {
                // Line comments are handled in skip_trivia; a lone `/` is division.
                Ok(self.simple(TokenKind::Slash, "/"))
            }
            '=' => {
                self.bump();
                if self.peek() == Some('=') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::EqEq,
                        Span::new(start, self.position()),
                        "==",
                    ))
                } else {
                    Ok(Token::new(
                        TokenKind::Eq,
                        Span::new(start, self.position()),
                        "=",
                    ))
                }
            }
            '!' => {
                self.bump();
                if self.peek() == Some('=') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::BangEq,
                        Span::new(start, self.position()),
                        "!=",
                    ))
                } else {
                    Ok(Token::new(
                        TokenKind::Bang,
                        Span::new(start, self.position()),
                        "!",
                    ))
                }
            }
            '<' => {
                self.bump();
                if self.peek() == Some('=') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::LtEq,
                        Span::new(start, self.position()),
                        "<=",
                    ))
                } else {
                    Ok(Token::new(
                        TokenKind::Lt,
                        Span::new(start, self.position()),
                        "<",
                    ))
                }
            }
            '>' => {
                self.bump();
                if self.peek() == Some('=') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::GtEq,
                        Span::new(start, self.position()),
                        ">=",
                    ))
                } else {
                    Ok(Token::new(
                        TokenKind::Gt,
                        Span::new(start, self.position()),
                        ">",
                    ))
                }
            }
            '&' => {
                self.bump();
                if self.peek() == Some('&') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::AmpAmp,
                        Span::new(start, self.position()),
                        "&&",
                    ))
                } else {
                    Err(error_at(
                        "expected `&&`, found lone `&`",
                        start,
                        self.source,
                    ))
                }
            }
            '|' => {
                self.bump();
                if self.peek() == Some('|') {
                    self.bump();
                    Ok(Token::new(
                        TokenKind::PipePipe,
                        Span::new(start, self.position()),
                        "||",
                    ))
                } else {
                    Err(error_at(
                        "expected `||`, found lone `|`",
                        start,
                        self.source,
                    ))
                }
            }
            '"' => self.string_literal(start),
            c if c.is_ascii_digit() => self.integer_literal(start),
            c if is_ident_start(c) => self.identifier_or_keyword(start),
            c => Err(error_at(
                format!("unexpected character `{c}`"),
                start,
                self.source,
            )),
        }
    }

    fn skip_trivia(&mut self) -> Result<(), LexError> {
        loop {
            match self.peek() {
                Some(c) if c.is_whitespace() => {
                    self.bump();
                }
                Some('/') if self.peek_at(1) == Some('/') => {
                    self.bump(); // /
                    self.bump(); // /
                    while let Some(c) = self.peek() {
                        if c == '\n' {
                            break;
                        }
                        self.bump();
                    }
                }
                Some('/') if self.peek_at(1) == Some('*') => {
                    let start = self.position();
                    self.bump(); // /
                    self.bump(); // *
                    loop {
                        match self.peek() {
                            None => {
                                return Err(error_at(
                                    "unterminated block comment",
                                    start,
                                    self.source,
                                ));
                            }
                            Some('*') if self.peek_at(1) == Some('/') => {
                                self.bump();
                                self.bump();
                                break;
                            }
                            Some(_) => {
                                self.bump();
                            }
                        }
                    }
                }
                _ => return Ok(()),
            }
        }
    }

    fn identifier_or_keyword(&mut self, start: Position) -> Result<Token, LexError> {
        let mut lexeme = String::new();
        while let Some(c) = self.peek() {
            if is_ident_continue(c) {
                lexeme.push(c);
                self.bump();
            } else {
                break;
            }
        }
        let span = Span::new(start, self.position());
        let kind = TokenKind::keyword(&lexeme).unwrap_or_else(|| TokenKind::Ident(lexeme.clone()));
        Ok(Token::new(kind, span, lexeme))
    }

    fn integer_literal(&mut self, start: Position) -> Result<Token, LexError> {
        let mut lexeme = String::new();
        while let Some(c) = self.peek() {
            if c.is_ascii_digit() {
                lexeme.push(c);
                self.bump();
            } else {
                break;
            }
        }

        // Disallow identifiers glued to numbers: `123abc`
        if let Some(c) = self.peek() {
            if is_ident_start(c) {
                return Err(error_at(
                    format!("invalid number `{lexeme}{c}…`"),
                    start,
                    self.source,
                ));
            }
        }

        let span = Span::new(start, self.position());
        let value: i64 = lexeme.parse().map_err(|_| {
            LexError::new(
                format!("integer literal `{lexeme}` is out of range for i64"),
                span,
                self.source,
            )
        })?;
        Ok(Token::new(TokenKind::IntLit(value), span, lexeme))
    }

    fn string_literal(&mut self, start: Position) -> Result<Token, LexError> {
        self.bump(); // opening "
        let mut value = String::new();
        let mut lexeme = String::from("\"");

        loop {
            match self.peek() {
                None => {
                    return Err(error_at("unterminated string literal", start, self.source));
                }
                Some('\n') => {
                    return Err(error_at(
                        "unterminated string literal (newline in string)",
                        start,
                        self.source,
                    ));
                }
                Some('"') => {
                    lexeme.push('"');
                    self.bump();
                    break;
                }
                Some('\\') => {
                    lexeme.push('\\');
                    self.bump();
                    let esc_pos = self.position();
                    match self.peek() {
                        Some(c) => {
                            lexeme.push(c);
                            self.bump();
                            let decoded = match c {
                                'n' => '\n',
                                't' => '\t',
                                'r' => '\r',
                                '\\' => '\\',
                                '"' => '"',
                                other => {
                                    return Err(error_at(
                                        format!("unknown string escape `\\{other}`"),
                                        esc_pos,
                                        self.source,
                                    ));
                                }
                            };
                            value.push(decoded);
                        }
                        None => {
                            return Err(error_at(
                                "unterminated string escape",
                                esc_pos,
                                self.source,
                            ));
                        }
                    }
                }
                Some(c) => {
                    lexeme.push(c);
                    value.push(c);
                    self.bump();
                }
            }
        }

        let span = Span::new(start, self.position());
        Ok(Token::new(TokenKind::StringLit(value), span, lexeme))
    }

    fn simple(&mut self, kind: TokenKind, lexeme: &str) -> Token {
        let start = self.position();
        self.bump();
        Token::new(kind, Span::new(start, self.position()), lexeme)
    }

    fn peek(&self) -> Option<char> {
        self.chars.get(self.index).copied()
    }

    fn peek_at(&self, offset: usize) -> Option<char> {
        self.chars.get(self.index + offset).copied()
    }

    fn bump(&mut self) -> Option<char> {
        let ch = self.chars.get(self.index).copied()?;
        self.index += 1;
        if ch == '\n' {
            self.line += 1;
            self.column = 1;
        } else {
            self.column += 1;
        }
        Some(ch)
    }

    fn position(&self) -> Position {
        Position::new(self.line, self.column)
    }
}

fn is_ident_start(c: char) -> bool {
    c.is_ascii_alphabetic() || c == '_'
}

fn is_ident_continue(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::token::TokenKind::*;

    fn kinds(source: &str) -> Vec<TokenKind> {
        tokenize(source)
            .expect("lex ok")
            .into_iter()
            .map(|t| t.kind)
            .collect()
    }

    #[test]
    fn empty_source_is_eof() {
        assert_eq!(kinds(""), vec![Eof]);
    }

    #[test]
    fn keywords_and_idents() {
        assert_eq!(
            kinds("let fn if else while return true false int bool string print"),
            vec![
                Let, Fn, If, Else, While, Return, True, False, Int, Bool, String,
                Ident("print".into()),
                Eof
            ]
        );
    }

    #[test]
    fn operators_and_punctuation() {
        assert_eq!(
            kinds("+ - * / % == != < <= > >= = ! && || -> ( ) { } , : ;"),
            vec![
                Plus, Minus, Star, Slash, Percent, EqEq, BangEq, Lt, LtEq, Gt, GtEq, Eq, Bang,
                AmpAmp, PipePipe, Arrow, LParen, RParen, LBrace, RBrace, Comma, Colon, Semi, Eof
            ]
        );
    }

    #[test]
    fn integers_and_strings() {
        let tokens = tokenize(r#"42 "hello\n" "a\"b""#).expect("lex ok");
        assert_eq!(tokens[0].kind, IntLit(42));
        assert_eq!(tokens[1].kind, StringLit("hello\n".into()));
        assert_eq!(tokens[2].kind, StringLit("a\"b".into()));
    }

    #[test]
    fn skips_line_and_block_comments() {
        let src = "let // comment\nx /* block */ = 1;";
        assert_eq!(
            kinds(src),
            vec![Let, Ident("x".into()), Eq, IntLit(1), Semi, Eof]
        );
    }

    #[test]
    fn fib_snippet() {
        let src = r#"
fn fib(n: int) -> int {
    if n < 2 { return n; }
    return fib(n - 1) + fib(n - 2);
}
print(fib(10));
"#;
        let ks = kinds(src);
        assert!(ks.contains(&Fn));
        assert!(ks.contains(&Arrow));
        assert!(ks.contains(&Ident("fib".into())));
        assert!(ks.contains(&Ident("print".into())));
        assert!(ks.contains(&IntLit(10)));
        assert_eq!(ks.last(), Some(&Eof));
    }

    #[test]
    fn tracks_line_and_column() {
        let tokens = tokenize("a\n  b").expect("lex ok");
        assert_eq!(tokens[0].span.start, Position::new(1, 1));
        assert_eq!(tokens[1].span.start, Position::new(2, 3));
    }

    #[test]
    fn unexpected_char_reports_location() {
        let err = tokenize("let x = @;").expect_err("should fail");
        assert!(err.message.contains('`'));
        assert_eq!(err.span.start, Position::new(1, 9));
        assert!(err.formatted.contains("--> 1:9"));
        assert!(err.formatted.contains('^'));
    }

    #[test]
    fn unterminated_string() {
        let err = tokenize("\"oops").expect_err("should fail");
        assert!(err.message.contains("unterminated"));
        assert_eq!(err.span.start, Position::new(1, 1));
    }

    #[test]
    fn unknown_escape() {
        let err = tokenize(r#""\q""#).expect_err("should fail");
        assert!(err.message.contains("unknown string escape"));
    }

    #[test]
    fn lone_ampersand() {
        let err = tokenize("&").expect_err("should fail");
        assert!(err.message.contains("&&"));
    }

    #[test]
    fn unterminated_block_comment() {
        let err = tokenize("/* never ends").expect_err("should fail");
        assert!(err.message.contains("unterminated block comment"));
    }
}
