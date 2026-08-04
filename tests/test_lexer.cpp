#include "tiny/lexer.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace tiny;

static std::vector<TokenKind> kinds(std::string_view source) {
    auto res = tokenize(source);
    if (!std::holds_alternative<std::vector<Token>>(res)) {
        std::cerr << "lex error: " << std::get<LexError>(res).formatted << std::endl;
        assert(false && "lex failed");
    }
    std::vector<TokenKind> ks;
    for (const auto& t : std::get<std::vector<Token>>(res)) {
        ks.push_back(t.kind);
    }
    return ks;
}

void test_empty_source_is_eof() {
    auto ks = kinds("");
    assert(ks.size() == 1);
    assert(ks[0].tag == TokenTag::Eof);
}

void test_keywords_and_idents() {
    auto ks = kinds("let fn if else while return true false int bool string print");
    std::vector<TokenTag> expected = {
        TokenTag::Let, TokenTag::Fn, TokenTag::If, TokenTag::Else,
        TokenTag::While, TokenTag::Return, TokenTag::True, TokenTag::False,
        TokenTag::Int, TokenTag::Bool, TokenTag::String, TokenTag::Ident,
        TokenTag::Eof
    };
    assert(ks.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert(ks[i].tag == expected[i]);
    }
    assert(ks[11].as_string() == "print");
}

void test_operators_and_punctuation() {
    auto ks = kinds("+ - * / % == != < <= > >= = ! && || -> ( ) { } , : ;");
    std::vector<TokenTag> expected = {
        TokenTag::Plus, TokenTag::Minus, TokenTag::Star, TokenTag::Slash, TokenTag::Percent,
        TokenTag::EqEq, TokenTag::BangEq, TokenTag::Lt, TokenTag::LtEq, TokenTag::Gt, TokenTag::GtEq,
        TokenTag::Eq, TokenTag::Bang, TokenTag::AmpAmp, TokenTag::PipePipe, TokenTag::Arrow,
        TokenTag::LParen, TokenTag::RParen, TokenTag::LBrace, TokenTag::RBrace,
        TokenTag::Comma, TokenTag::Colon, TokenTag::Semi, TokenTag::Eof
    };
    assert(ks.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert(ks[i].tag == expected[i]);
    }
}

void test_integers_and_strings() {
    auto res = tokenize("42 \"hello\\n\" \"a\\\"b\"");
    assert(std::holds_alternative<std::vector<Token>>(res));
    const auto& tokens = std::get<std::vector<Token>>(res);
    assert(tokens.size() == 4); // 42, str1, str2, eof
    assert(tokens[0].kind.tag == TokenTag::IntLit && tokens[0].kind.as_int() == 42);
    assert(tokens[1].kind.tag == TokenTag::StringLit && tokens[1].kind.as_string() == "hello\n");
    assert(tokens[2].kind.tag == TokenTag::StringLit && tokens[2].kind.as_string() == "a\"b");
}

void test_skips_line_and_block_comments() {
    auto ks = kinds("let // comment\nx /* block */ = 1;");
    assert(ks.size() == 6);
    assert(ks[0].tag == TokenTag::Let);
    assert(ks[1].tag == TokenTag::Ident && ks[1].as_string() == "x");
    assert(ks[2].tag == TokenTag::Eq);
    assert(ks[3].tag == TokenTag::IntLit && ks[3].as_int() == 1);
    assert(ks[4].tag == TokenTag::Semi);
    assert(ks[5].tag == TokenTag::Eof);
}

void test_fib_snippet() {
    const char* src = R"(
fn fib(n: int) -> int {
    if n < 2 { return n; }
    return fib(n - 1) + fib(n - 2);
}
print(fib(10));
)";
    auto ks = kinds(src);
    auto contains_tag = [&](TokenTag tag) {
        return std::any_of(ks.begin(), ks.end(), [&](const TokenKind& k) { return k.tag == tag; });
    };
    assert(contains_tag(TokenTag::Fn));
    assert(contains_tag(TokenTag::Arrow));
    assert(std::any_of(ks.begin(), ks.end(), [&](const TokenKind& k) {
        return k.tag == TokenTag::Ident && k.as_string() == "fib";
    }));
    assert(std::any_of(ks.begin(), ks.end(), [&](const TokenKind& k) {
        return k.tag == TokenTag::Ident && k.as_string() == "print";
    }));
    assert(std::any_of(ks.begin(), ks.end(), [&](const TokenKind& k) {
        return k.tag == TokenTag::IntLit && k.as_int() == 10;
    }));
    assert(ks.back().tag == TokenTag::Eof);
}

void test_tracks_line_and_column() {
    auto res = tokenize("a\n  b");
    assert(std::holds_alternative<std::vector<Token>>(res));
    const auto& tokens = std::get<std::vector<Token>>(res);
    assert(tokens[0].span.start == Position(1, 1));
    assert(tokens[1].span.start == Position(2, 3));
}

void test_unexpected_char_reports_location() {
    auto res = tokenize("let x = @;");
    assert(std::holds_alternative<LexError>(res));
    const auto& err = std::get<LexError>(res);
    assert(err.message.find('`') != std::string::npos);
    assert(err.span.start == Position(1, 9));
    assert(err.formatted.find("--> 1:9") != std::string::npos);
    assert(err.formatted.find('^') != std::string::npos);
}

void test_unterminated_string() {
    auto res = tokenize("\"oops");
    assert(std::holds_alternative<LexError>(res));
    const auto& err = std::get<LexError>(res);
    assert(err.message.find("unterminated") != std::string::npos);
    assert(err.span.start == Position(1, 1));
}

void test_unknown_escape() {
    auto res = tokenize("\"\\q\"");
    assert(std::holds_alternative<LexError>(res));
    const auto& err = std::get<LexError>(res);
    assert(err.message.find("unknown string escape") != std::string::npos);
}

void test_lone_ampersand() {
    auto res = tokenize("&");
    assert(std::holds_alternative<LexError>(res));
    const auto& err = std::get<LexError>(res);
    assert(err.message.find("&&") != std::string::npos);
}

void test_unterminated_block_comment() {
    auto res = tokenize("/* never ends");
    assert(std::holds_alternative<LexError>(res));
    const auto& err = std::get<LexError>(res);
    assert(err.message.find("unterminated block comment") != std::string::npos);
}

void test_diagnostic_points_at_column() {
    const char* src = "let x = @;\n";
    auto err = error_at("unexpected character `@`", Position(1, 9), src);
    assert(err.formatted.find("--> 1:9") != std::string::npos);
    assert(err.formatted.find("let x = @;") != std::string::npos);
    assert(err.formatted.find("        ^") != std::string::npos);
}

int main() {
    test_empty_source_is_eof();
    test_keywords_and_idents();
    test_operators_and_punctuation();
    test_integers_and_strings();
    test_skips_line_and_block_comments();
    test_fib_snippet();
    test_tracks_line_and_column();
    test_unexpected_char_reports_location();
    test_unterminated_string();
    test_unknown_escape();
    test_lone_ampersand();
    test_unterminated_block_comment();
    test_diagnostic_points_at_column();
    std::cout << "All lexer tests passed (13/13)!" << std::endl;
    return 0;
}
