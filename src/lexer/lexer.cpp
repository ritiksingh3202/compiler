#include "tiny/lexer.hpp"
#include <cctype>
#include <charconv>

namespace tiny {

static bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool is_ident_continue(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

Lexer::Lexer(std::string_view source)
    : source_(source),
      chars_(source.begin(), source.end()),
      index_(0),
      line_(1),
      column_(1) {}

std::optional<char> Lexer::peek() const {
    if (index_ < chars_.size()) {
        return chars_[index_];
    }
    return std::nullopt;
}

std::optional<char> Lexer::peek_at(size_t offset) const {
    if (index_ + offset < chars_.size()) {
        return chars_[index_ + offset];
    }
    return std::nullopt;
}

std::optional<char> Lexer::bump() {
    if (index_ >= chars_.size()) {
        return std::nullopt;
    }
    char ch = chars_[index_++];
    if (ch == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return ch;
}

Position Lexer::position() const {
    return Position(line_, column_);
}

Token Lexer::simple(TokenKind kind, std::string_view lexeme) {
    Position start = position();
    bump();
    return Token(std::move(kind), Span(start, position()), std::string(lexeme));
}

std::optional<LexError> Lexer::skip_trivia() {
    while (true) {
        auto ch = peek();
        if (!ch) {
            return std::nullopt;
        }

        if (std::isspace(static_cast<unsigned char>(*ch))) {
            bump();
            continue;
        }

        if (*ch == '/' && peek_at(1) == '/') {
            bump(); // /
            bump(); // /
            while (auto c = peek()) {
                if (*c == '\n') {
                    break;
                }
                bump();
            }
            continue;
        }

        if (*ch == '/' && peek_at(1) == '*') {
            Position start = position();
            bump(); // /
            bump(); // *
            while (true) {
                auto c = peek();
                if (!c) {
                    return error_at("unterminated block comment", start, source_);
                }
                if (*c == '*' && peek_at(1) == '/') {
                    bump(); // *
                    bump(); // /
                    break;
                }
                bump();
            }
            continue;
        }

        return std::nullopt;
    }
}

std::variant<Token, LexError> Lexer::identifier_or_keyword(Position start) {
    std::string lexeme;
    while (auto c = peek()) {
        if (is_ident_continue(*c)) {
            lexeme += *c;
            bump();
        } else {
            break;
        }
    }
    Span span(start, position());
    auto kw = TokenKind::keyword(lexeme);
    TokenKind kind = kw ? *kw : TokenKind::make_ident(lexeme);
    return Token(kind, span, std::move(lexeme));
}

std::variant<Token, LexError> Lexer::integer_literal(Position start) {
    std::string lexeme;
    while (auto c = peek()) {
        if (std::isdigit(static_cast<unsigned char>(*c))) {
            lexeme += *c;
            bump();
        } else {
            break;
        }
    }

    if (auto c = peek()) {
        if (is_ident_start(*c)) {
            return error_at("invalid number `" + lexeme + *c + "…`", start, source_);
        }
    }

    Span span(start, position());
    int64_t val = 0;
    auto [ptr, ec] = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), val);
    if (ec != std::errc()) {
        return LexError("integer literal `" + lexeme + "` is out of range for i64", span, source_);
    }
    return Token(TokenKind::make_int(val), span, std::move(lexeme));
}

std::variant<Token, LexError> Lexer::string_literal(Position start) {
    bump(); // opening "
    std::string value;
    std::string lexeme = "\"";

    while (true) {
        auto c = peek();
        if (!c) {
            return error_at("unterminated string literal", start, source_);
        }
        if (*c == '\n') {
            return error_at("unterminated string literal (newline in string)", start, source_);
        }
        if (*c == '"') {
            lexeme += '"';
            bump();
            break;
        }
        if (*c == '\\') {
            lexeme += '\\';
            bump();
            Position esc_pos = position();
            auto next_c = peek();
            if (!next_c) {
                return error_at("unterminated string escape", esc_pos, source_);
            }
            lexeme += *next_c;
            bump();
            char decoded = '\0';
            switch (*next_c) {
                case 'n': decoded = '\n'; break;
                case 't': decoded = '\t'; break;
                case 'r': decoded = '\r'; break;
                case '\\': decoded = '\\'; break;
                case '"': decoded = '"'; break;
                default:
                    return error_at(std::string("unknown string escape `\\") + *next_c + "`", esc_pos, source_);
            }
            value += decoded;
            continue;
        }

        lexeme += *c;
        value += *c;
        bump();
    }

    Span span(start, position());
    return Token(TokenKind::make_string(value), span, std::move(lexeme));
}

std::variant<Token, LexError> Lexer::next_token() {
    if (auto err = skip_trivia()) {
        return *err;
    }

    Position start = position();
    auto ch = peek();
    if (!ch) {
        return Token(TokenKind(TokenTag::Eof), Span::empty(start), "");
    }

    switch (*ch) {
        case '(': return simple(TokenKind(TokenTag::LParen), "(");
        case ')': return simple(TokenKind(TokenTag::RParen), ")");
        case '{': return simple(TokenKind(TokenTag::LBrace), "{");
        case '}': return simple(TokenKind(TokenTag::RBrace), "}");
        case ',': return simple(TokenKind(TokenTag::Comma), ",");
        case ':': return simple(TokenKind(TokenTag::Colon), ":");
        case ';': return simple(TokenKind(TokenTag::Semi), ";");
        case '+': return simple(TokenKind(TokenTag::Plus), "+");
        case '*': return simple(TokenKind(TokenTag::Star), "*");
        case '%': return simple(TokenKind(TokenTag::Percent), "%");
        case '/': return simple(TokenKind(TokenTag::Slash), "/");
        case '-': {
            bump();
            if (peek() == '>') {
                bump();
                return Token(TokenKind(TokenTag::Arrow), Span(start, position()), "->");
            }
            return Token(TokenKind(TokenTag::Minus), Span(start, position()), "-");
        }
        case '=': {
            bump();
            if (peek() == '=') {
                bump();
                return Token(TokenKind(TokenTag::EqEq), Span(start, position()), "==");
            }
            return Token(TokenKind(TokenTag::Eq), Span(start, position()), "=");
        }
        case '!': {
            bump();
            if (peek() == '=') {
                bump();
                return Token(TokenKind(TokenTag::BangEq), Span(start, position()), "!=");
            }
            return Token(TokenKind(TokenTag::Bang), Span(start, position()), "!");
        }
        case '<': {
            bump();
            if (peek() == '=') {
                bump();
                return Token(TokenKind(TokenTag::LtEq), Span(start, position()), "<=");
            }
            return Token(TokenKind(TokenTag::Lt), Span(start, position()), "<");
        }
        case '>': {
            bump();
            if (peek() == '=') {
                bump();
                return Token(TokenKind(TokenTag::GtEq), Span(start, position()), ">=");
            }
            return Token(TokenKind(TokenTag::Gt), Span(start, position()), ">");
        }
        case '&': {
            bump();
            if (peek() == '&') {
                bump();
                return Token(TokenKind(TokenTag::AmpAmp), Span(start, position()), "&&");
            }
            return error_at("expected `&&`, found lone `&`", start, source_);
        }
        case '|': {
            bump();
            if (peek() == '|') {
                bump();
                return Token(TokenKind(TokenTag::PipePipe), Span(start, position()), "||");
            }
            return error_at("expected `||`, found lone `|`", start, source_);
        }
        case '"':
            return string_literal(start);
        default:
            if (std::isdigit(static_cast<unsigned char>(*ch))) {
                return integer_literal(start);
            }
            if (is_ident_start(*ch)) {
                return identifier_or_keyword(start);
            }
            return error_at(std::string("unexpected character `") + *ch + "`", start, source_);
    }
}

std::variant<std::vector<Token>, LexError> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        auto res = next_token();
        if (std::holds_alternative<LexError>(res)) {
            return std::get<LexError>(res);
        }
        auto token = std::get<Token>(std::move(res));
        bool is_eof = (token.kind.tag == TokenTag::Eof);
        tokens.push_back(std::move(token));
        if (is_eof) {
            break;
        }
    }
    return tokens;
}

std::variant<std::vector<Token>, LexError> tokenize(std::string_view source) {
    return Lexer(source).tokenize();
}

} // namespace tiny
