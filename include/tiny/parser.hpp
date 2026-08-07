#pragma once

#include "tiny/ast.hpp"
#include "tiny/lexer.hpp"
#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <variant>

namespace tiny {

struct ParseError {
    std::string message;
    Span span;
    std::optional<std::string> expected;
    std::optional<std::string> found;
    std::string formatted;

    ParseError(std::string msg, Span s, std::string_view source,
               std::optional<std::string> exp = std::nullopt,
               std::optional<std::string> fnd = std::nullopt);

    static ParseError unexpected(Span span, std::string_view source,
                                 std::string_view expected, const TokenKind& found);

    static ParseError from_lex(const LexError& err);
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens, std::string_view source);

    std::variant<Program, ParseError> parse_program();

private:
    const std::vector<Token>& tokens_;
    size_t pos_{0};
    std::string_view source_;

    const Token& peek() const;
    bool is_at_end() const;
    bool check(TokenTag tag) const;
    Token advance();
    bool match_kind(TokenTag tag);
    std::variant<Token, ParseError> expect_kind(TokenTag tag, std::string_view expected);
    std::variant<std::pair<std::string, Span>, ParseError> expect_ident(std::string_view expected);
    Position previous_end(Position fallback) const;

    std::variant<FnDecl, ParseError> parse_fn_decl();
    std::variant<Param, ParseError> parse_param();
    std::variant<Type, ParseError> parse_type();

    std::variant<Stmt, ParseError> parse_stmt();
    std::variant<Stmt, ParseError> parse_let();
    std::variant<Stmt, ParseError> parse_return();
    std::variant<Stmt, ParseError> parse_if();
    std::variant<Stmt, ParseError> parse_while();
    std::variant<Block, ParseError> parse_block();
    std::variant<Stmt, ParseError> parse_expr_stmt();

    std::variant<Expr, ParseError> parse_expr(uint8_t min_bp);
    std::variant<Expr, ParseError> parse_prefix();
    std::variant<Expr, ParseError> parse_call_or_primary();
    std::variant<Expr, ParseError> parse_primary();
};

std::variant<Program, ParseError> parse(std::string_view source);

} // namespace tiny
