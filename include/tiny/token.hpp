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
    TokenTag tag;
    std::variant<std::monostate, std::string, int64_t> payload;

    TokenKind() : tag(TokenTag::Eof), payload(std::monostate{}) {}
    TokenKind(TokenTag t) : tag(t), payload(std::monostate{}) {}
    TokenKind(TokenTag t, std::string s) : tag(t), payload(std::move(s)) {}
    TokenKind(TokenTag t, int64_t n) : tag(t), payload(n) {}

    static TokenKind make_ident(std::string name) {
        return TokenKind(TokenTag::Ident, std::move(name));
    }

    static TokenKind make_int(int64_t n) {
        return TokenKind(TokenTag::IntLit, n);
    }

    static TokenKind make_string(std::string s) {
        return TokenKind(TokenTag::StringLit, std::move(s));
    }

    static std::optional<TokenKind> keyword(std::string_view ident) {
        if (ident == "let") return TokenKind(TokenTag::Let);
        if (ident == "fn") return TokenKind(TokenTag::Fn);
        if (ident == "if") return TokenKind(TokenTag::If);
        if (ident == "else") return TokenKind(TokenTag::Else);
        if (ident == "while") return TokenKind(TokenTag::While);
        if (ident == "return") return TokenKind(TokenTag::Return);
        if (ident == "true") return TokenKind(TokenTag::True);
        if (ident == "false") return TokenKind(TokenTag::False);
        if (ident == "int") return TokenKind(TokenTag::Int);
        if (ident == "bool") return TokenKind(TokenTag::Bool);
        if (ident == "string") return TokenKind(TokenTag::String);
        return std::nullopt;
    }

    bool is_keyword() const {
        switch (tag) {
            case TokenTag::Let:
            case TokenTag::Fn:
            case TokenTag::If:
            case TokenTag::Else:
            case TokenTag::While:
            case TokenTag::Return:
            case TokenTag::True:
            case TokenTag::False:
            case TokenTag::Int:
            case TokenTag::Bool:
            case TokenTag::String:
                return true;
            default:
                return false;
        }
    }

    const std::string& as_string() const {
        return std::get<std::string>(payload);
    }

    int64_t as_int() const {
        return std::get<int64_t>(payload);
    }

    bool operator==(const TokenKind& other) const = default;
    bool operator!=(const TokenKind& other) const = default;

    std::string to_string() const {
        switch (tag) {
            case TokenTag::Let: return "`let`";
            case TokenTag::Fn: return "`fn`";
            case TokenTag::If: return "`if`";
            case TokenTag::Else: return "`else`";
            case TokenTag::While: return "`while`";
            case TokenTag::Return: return "`return`";
            case TokenTag::True: return "`true`";
            case TokenTag::False: return "`false`";
            case TokenTag::Int: return "`int`";
            case TokenTag::Bool: return "`bool`";
            case TokenTag::String: return "`string`";
            case TokenTag::Ident: return "identifier `" + as_string() + "`";
            case TokenTag::IntLit: return "integer `" + std::to_string(as_int()) + "`";
            case TokenTag::StringLit: {
                std::string s = as_string();
                std::string esc = "\"";
                for (char c : s) {
                    if (c == '\\') esc += "\\\\";
                    else if (c == '"') esc += "\\\"";
                    else if (c == '\n') esc += "\\n";
                    else if (c == '\t') esc += "\\t";
                    else if (c == '\r') esc += "\\r";
                    else esc += c;
                }
                esc += "\"";
                return "string " + esc;
            }
            case TokenTag::Plus: return "`+`";
            case TokenTag::Minus: return "`-`";
            case TokenTag::Star: return "`*`";
            case TokenTag::Slash: return "`/`";
            case TokenTag::Percent: return "`%`";
            case TokenTag::EqEq: return "`==`";
            case TokenTag::BangEq: return "`!=`";
            case TokenTag::Lt: return "`<`";
            case TokenTag::LtEq: return "`<=`";
            case TokenTag::Gt: return "`>`";
            case TokenTag::GtEq: return "`>=`";
            case TokenTag::Eq: return "`=`";
            case TokenTag::Bang: return "`!`";
            case TokenTag::AmpAmp: return "`&&`";
            case TokenTag::PipePipe: return "`||`";
            case TokenTag::Arrow: return "`->`";
            case TokenTag::LParen: return "`(`";
            case TokenTag::RParen: return "`)`";
            case TokenTag::LBrace: return "`{`";
            case TokenTag::RBrace: return "`}`";
            case TokenTag::Comma: return "`,`";
            case TokenTag::Colon: return "`:`";
            case TokenTag::Semi: return "`;`";
            case TokenTag::Eof: return "`<eof>`";
        }
        return "`?`";
    }
};

struct Token {
    TokenKind kind;
    Span span;
    std::string lexeme;

    Token(TokenKind k, Span s, std::string lex)
        : kind(std::move(k)), span(s), lexeme(std::move(lex)) {}

    bool operator==(const Token& other) const = default;
};

} // namespace tiny
