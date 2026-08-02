#pragma once

#include "tiny/span.hpp"
#include <string>
#include <variant>
#include <optional>
#include <sstream>

namespace tiny {

enum class TokenTag {
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
    Ident,
    IntLit,
    StringLit,

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

    Eof
};

struct TokenKind {
