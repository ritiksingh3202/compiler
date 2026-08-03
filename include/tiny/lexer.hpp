#pragma once

#include "tiny/span.hpp"
#include "tiny/token.hpp"
#include <string>
#include <vector>
#include <string_view>
#include <variant>

namespace tiny {

struct LexError {
    std::string message;
    Span span;
    std::string formatted;

    LexError(std::string msg, Span s, std::string_view source)
        : message(std::move(msg)), span(s), formatted(format_diagnostic(message, span, source)) {}
};

inline LexError error_at(std::string msg, Position pos, std::string_view source) {
    return LexError(std::move(msg), Span::empty(pos), source);
}

class Lexer {
public:
    explicit Lexer(std::string_view source);

    std::variant<std::vector<Token>, LexError> tokenize();

private:
    std::string_view source_;
    std::vector<char> chars_;
    size_t index_{0};
    uint32_t line_{1};
    uint32_t column_{1};

    std::optional<char> peek() const;
    std::optional<char> peek_at(size_t offset) const;
    std::optional<char> bump();
    Position position() const;

    Token simple(TokenKind kind, std::string_view lexeme);
    std::optional<LexError> skip_trivia();
    std::variant<Token, LexError> next_token();
    std::variant<Token, LexError> identifier_or_keyword(Position start);
    std::variant<Token, LexError> integer_literal(Position start);
    std::variant<Token, LexError> string_literal(Position start);
};

std::variant<std::vector<Token>, LexError> tokenize(std::string_view source);

} // namespace tiny
