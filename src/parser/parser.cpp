#include "tiny/parser.hpp"
#include <sstream>

namespace tiny {

ParseError::ParseError(std::string msg, Span s, std::string_view source,
                       std::optional<std::string> exp,
                       std::optional<std::string> fnd)
    : message(std::move(msg)), span(s), expected(std::move(exp)), found(std::move(fnd)) {
    std::string detail;
    if (expected && found) {
        detail = message + " (expected " + *expected + ", found " + *found + ")";
    } else if (expected) {
        detail = message + " (expected " + *expected + ")";
    } else if (found) {
        detail = message + " (found " + *found + ")";
    } else {
        detail = message;
    }
    formatted = format_diagnostic(detail, span, source);
}

ParseError ParseError::unexpected(Span span, std::string_view source,
                                 std::string_view expected, const TokenKind& found) {
    std::string exp_str(expected);
    std::string fnd_str = found.to_string();
    return ParseError("expected " + exp_str + ", found " + fnd_str,
                      span, source, std::move(exp_str), std::move(fnd_str));
}

ParseError ParseError::from_lex(const LexError& err) {
    ParseError pe(err.message, err.span, "");
    pe.formatted = err.formatted;
    return pe;
}

Parser::Parser(const std::vector<Token>& tokens, std::string_view source)
    : tokens_(tokens), pos_(0), source_(source) {}

const Token& Parser::peek() const {
    if (pos_ < tokens_.size()) {
        return tokens_[pos_];
    }
    return tokens_.back();
}

bool Parser::is_at_end() const {
    return peek().kind.tag == TokenTag::Eof;
}

bool Parser::check(TokenTag tag) const {
    return peek().kind.tag == tag;
}

Token Parser::advance() {
    Token tok = peek();
    if (!is_at_end()) {
        pos_++;
    }
    return tok;
}

bool Parser::match_kind(TokenTag tag) {
    if (check(tag)) {
        advance();
        return true;
    }
    return false;
}

std::variant<Token, ParseError> Parser::expect_kind(TokenTag tag, std::string_view expected) {
    if (check(tag)) {
        return advance();
    }
    return ParseError::unexpected(peek().span, source_, expected, peek().kind);
}

std::variant<std::pair<std::string, Span>, ParseError> Parser::expect_ident(std::string_view expected) {
    const Token& tok = peek();
    if (tok.kind.tag == TokenTag::Ident) {
        std::string name = tok.kind.as_string();
        Span sp = tok.span;
        advance();
        return std::make_pair(std::move(name), sp);
    }
    return ParseError::unexpected(tok.span, source_, expected, tok.kind);
}

Position Parser::previous_end(Position fallback) const {
    if (pos_ == 0) {
        return fallback;
    }
    return tokens_[pos_ - 1].span.end;
}

std::variant<Program, ParseError> Parser::parse_program() {
    Position start = peek().span.start;
    std::vector<FnDecl> functions;
    std::vector<Stmt> statements;

    while (!is_at_end()) {
        if (check(TokenTag::Fn)) {
            auto fn_res = parse_fn_decl();
            if (std::holds_alternative<ParseError>(fn_res)) {
                return std::get<ParseError>(fn_res);
            }
            functions.push_back(std::get<FnDecl>(std::move(fn_res)));
        } else {
            auto stmt_res = parse_stmt();
            if (std::holds_alternative<ParseError>(stmt_res)) {
                return std::get<ParseError>(stmt_res);
            }
            statements.push_back(std::get<Stmt>(std::move(stmt_res)));
        }
    }

    Position end = previous_end(start);
    return Program{std::move(functions), std::move(statements), Span(start, end)};
}

std::variant<FnDecl, ParseError> Parser::parse_fn_decl() {
    Position start = peek().span.start;
    auto exp_fn = expect_kind(TokenTag::Fn, "`fn`");
    if (std::holds_alternative<ParseError>(exp_fn)) return std::get<ParseError>(exp_fn);

    auto exp_name = expect_ident("`function name`");
    if (std::holds_alternative<ParseError>(exp_name)) return std::get<ParseError>(exp_name);
    auto [name, _] = std::get<std::pair<std::string, Span>>(exp_name);

    auto exp_lp = expect_kind(TokenTag::LParen, "`(`");
    if (std::holds_alternative<ParseError>(exp_lp)) return std::get<ParseError>(exp_lp);

    std::vector<Param> params;
    if (!check(TokenTag::RParen)) {
        while (true) {
            auto p_res = parse_param();
            if (std::holds_alternative<ParseError>(p_res)) return std::get<ParseError>(p_res);
            params.push_back(std::get<Param>(std::move(p_res)));
            if (!match_kind(TokenTag::Comma)) {
                break;
            }
        }
    }

    auto exp_rp = expect_kind(TokenTag::RParen, "`)`");
    if (std::holds_alternative<ParseError>(exp_rp)) return std::get<ParseError>(exp_rp);

    Type return_type = Type::Void();
    if (match_kind(TokenTag::Arrow)) {
        auto ty_res = parse_type();
        if (std::holds_alternative<ParseError>(ty_res)) return std::get<ParseError>(ty_res);
        return_type = std::get<Type>(ty_res);
    }

    auto body_res = parse_block();
    if (std::holds_alternative<ParseError>(body_res)) return std::get<ParseError>(body_res);
    Block body = std::get<Block>(std::move(body_res));

    Position end = body.span.end;
    return FnDecl{std::move(name), std::move(params), return_type, std::move(body), Span(start, end)};
}

std::variant<Param, ParseError> Parser::parse_param() {
    auto exp_name = expect_ident("`parameter name`");
    if (std::holds_alternative<ParseError>(exp_name)) return std::get<ParseError>(exp_name);
    auto [name, name_span] = std::get<std::pair<std::string, Span>>(exp_name);

    auto exp_col = expect_kind(TokenTag::Colon, "`:`");
    if (std::holds_alternative<ParseError>(exp_col)) return std::get<ParseError>(exp_col);

    auto ty_res = parse_type();
    if (std::holds_alternative<ParseError>(ty_res)) return std::get<ParseError>(ty_res);
    Type ty = std::get<Type>(ty_res);

    Position end = previous_end(name_span.start);
    return Param{std::move(name), ty, Span(name_span.start, end)};
}

std::variant<Type, ParseError> Parser::parse_type() {
    const Token& tok = peek();
    Type ty;
    switch (tok.kind.tag) {
        case TokenTag::Int: ty = Type::Int(); break;
        case TokenTag::Bool: ty = Type::Bool(); break;
        case TokenTag::String: ty = Type::String(); break;
        default:
            return ParseError::unexpected(tok.span, source_, "type (`int`, `bool`, or `string`)", tok.kind);
    }
    advance();
    return ty;
}

std::variant<Stmt, ParseError> Parser::parse_stmt() {
    switch (peek().kind.tag) {
        case TokenTag::Let: return parse_let();
        case TokenTag::Return: return parse_return();
        case TokenTag::If: return parse_if();
        case TokenTag::While: return parse_while();
        case TokenTag::LBrace: {
            auto b_res = parse_block();
            if (std::holds_alternative<ParseError>(b_res)) return std::get<ParseError>(b_res);
            return Stmt::make_block(std::get<Block>(std::move(b_res)));
        }
        default: return parse_expr_stmt();
    }
}

std::variant<Stmt, ParseError> Parser::parse_let() {
    Position start = peek().span.start;
    auto exp_let = expect_kind(TokenTag::Let, "`let`");
    if (std::holds_alternative<ParseError>(exp_let)) return std::get<ParseError>(exp_let);

    auto exp_name = expect_ident("`variable name`");
    if (std::holds_alternative<ParseError>(exp_name)) return std::get<ParseError>(exp_name);
    auto [name, _] = std::get<std::pair<std::string, Span>>(exp_name);

    std::optional<Type> ty = std::nullopt;
    if (match_kind(TokenTag::Colon)) {
        auto ty_res = parse_type();
        if (std::holds_alternative<ParseError>(ty_res)) return std::get<ParseError>(ty_res);
        ty = std::get<Type>(ty_res);
    }

    auto exp_eq = expect_kind(TokenTag::Eq, "`=`");
    if (std::holds_alternative<ParseError>(exp_eq)) return std::get<ParseError>(exp_eq);

    auto init_res = parse_expr(0);
    if (std::holds_alternative<ParseError>(init_res)) return std::get<ParseError>(init_res);
    Expr init = std::get<Expr>(std::move(init_res));

    auto exp_semi = expect_kind(TokenTag::Semi, "`;`");
    if (std::holds_alternative<ParseError>(exp_semi)) return std::get<ParseError>(exp_semi);

    Position end = previous_end(start);
    return Stmt::make_let(std::move(name), ty, std::move(init), Span(start, end));
}

std::variant<Stmt, ParseError> Parser::parse_return() {
    Position start = peek().span.start;
    auto exp_ret = expect_kind(TokenTag::Return, "`return`");
    if (std::holds_alternative<ParseError>(exp_ret)) return std::get<ParseError>(exp_ret);

    std::optional<Expr> value = std::nullopt;
    if (!check(TokenTag::Semi)) {
        auto val_res = parse_expr(0);
        if (std::holds_alternative<ParseError>(val_res)) return std::get<ParseError>(val_res);
        value = std::get<Expr>(std::move(val_res));
    }

    auto exp_semi = expect_kind(TokenTag::Semi, "`;`");
    if (std::holds_alternative<ParseError>(exp_semi)) return std::get<ParseError>(exp_semi);

    Position end = previous_end(start);
    return Stmt::make_return(std::move(value), Span(start, end));
}

std::variant<Stmt, ParseError> Parser::parse_if() {
    Position start = peek().span.start;
    auto exp_if = expect_kind(TokenTag::If, "`if`");
    if (std::holds_alternative<ParseError>(exp_if)) return std::get<ParseError>(exp_if);

    auto cond_res = parse_expr(0);
    if (std::holds_alternative<ParseError>(cond_res)) return std::get<ParseError>(cond_res);
    Expr cond = std::get<Expr>(std::move(cond_res));

    auto then_res = parse_block();
    if (std::holds_alternative<ParseError>(then_res)) return std::get<ParseError>(then_res);
    Block then_block = std::get<Block>(std::move(then_res));

    std::optional<Block> else_block = std::nullopt;
    if (match_kind(TokenTag::Else)) {
        if (check(TokenTag::If)) {
            auto nested_res = parse_if();
            if (std::holds_alternative<ParseError>(nested_res)) return std::get<ParseError>(nested_res);
            Stmt nested = std::get<Stmt>(std::move(nested_res));
            Span sp = nested.span();
            std::vector<Stmt> stmts;
            stmts.push_back(std::move(nested));
            else_block = Block{std::move(stmts), sp};
        } else {
            auto eb_res = parse_block();
            if (std::holds_alternative<ParseError>(eb_res)) return std::get<ParseError>(eb_res);
            else_block = std::get<Block>(std::move(eb_res));
        }
    }

    Position end = else_block ? else_block->span.end : then_block.span.end;
    return Stmt::make_if(std::move(cond), std::move(then_block), std::move(else_block), Span(start, end));
}

std::variant<Stmt, ParseError> Parser::parse_while() {
    Position start = peek().span.start;
    auto exp_while = expect_kind(TokenTag::While, "`while`");
    if (std::holds_alternative<ParseError>(exp_while)) return std::get<ParseError>(exp_while);

    auto cond_res = parse_expr(0);
    if (std::holds_alternative<ParseError>(cond_res)) return std::get<ParseError>(cond_res);
    Expr cond = std::get<Expr>(std::move(cond_res));

    auto body_res = parse_block();
    if (std::holds_alternative<ParseError>(body_res)) return std::get<ParseError>(body_res);
    Block body = std::get<Block>(std::move(body_res));

    Position end = body.span.end;
    return Stmt::make_while(std::move(cond), std::move(body), Span(start, end));
}

std::variant<Block, ParseError> Parser::parse_block() {
    Position start = peek().span.start;
    auto exp_lb = expect_kind(TokenTag::LBrace, "`{`");
    if (std::holds_alternative<ParseError>(exp_lb)) return std::get<ParseError>(exp_lb);

    std::vector<Stmt> statements;
    while (!check(TokenTag::RBrace) && !is_at_end()) {
        auto stmt_res = parse_stmt();
        if (std::holds_alternative<ParseError>(stmt_res)) return std::get<ParseError>(stmt_res);
        statements.push_back(std::get<Stmt>(std::move(stmt_res)));
    }

    auto exp_rb = expect_kind(TokenTag::RBrace, "`}`");
    if (std::holds_alternative<ParseError>(exp_rb)) return std::get<ParseError>(exp_rb);

    Position end = previous_end(start);
    return Block{std::move(statements), Span(start, end)};
}

std::variant<Stmt, ParseError> Parser::parse_expr_stmt() {
    auto expr_res = parse_expr(0);
    if (std::holds_alternative<ParseError>(expr_res)) return std::get<ParseError>(expr_res);
    Expr expr = std::get<Expr>(std::move(expr_res));

    auto exp_semi = expect_kind(TokenTag::Semi, "`;`");
    if (std::holds_alternative<ParseError>(exp_semi)) return std::get<ParseError>(exp_semi);

    return Stmt::make_expr(std::move(expr));
}

enum class InfixKind {
    Assign,
    Binary
};

struct InfixInfo {
    uint8_t left_bp;
    uint8_t right_bp;
    InfixKind kind;
    BinaryOp op;
};

static std::optional<InfixInfo> infix_binding(TokenTag tag) {
    switch (tag) {
        case TokenTag::Eq:
            return InfixInfo{1, 1, InfixKind::Assign, BinaryOp::Add};
        case TokenTag::PipePipe:
            return InfixInfo{2, 3, InfixKind::Binary, BinaryOp::Or};
        case TokenTag::AmpAmp:
            return InfixInfo{3, 4, InfixKind::Binary, BinaryOp::And};
        case TokenTag::EqEq:
            return InfixInfo{4, 5, InfixKind::Binary, BinaryOp::Eq};
        case TokenTag::BangEq:
            return InfixInfo{4, 5, InfixKind::Binary, BinaryOp::Ne};
        case TokenTag::Lt:
            return InfixInfo{5, 6, InfixKind::Binary, BinaryOp::Lt};
        case TokenTag::LtEq:
            return InfixInfo{5, 6, InfixKind::Binary, BinaryOp::Le};
        case TokenTag::Gt:
            return InfixInfo{5, 6, InfixKind::Binary, BinaryOp::Gt};
        case TokenTag::GtEq:
            return InfixInfo{5, 6, InfixKind::Binary, BinaryOp::Ge};
        case TokenTag::Plus:
            return InfixInfo{6, 7, InfixKind::Binary, BinaryOp::Add};
        case TokenTag::Minus:
            return InfixInfo{6, 7, InfixKind::Binary, BinaryOp::Sub};
        case TokenTag::Star:
            return InfixInfo{7, 8, InfixKind::Binary, BinaryOp::Mul};
        case TokenTag::Slash:
            return InfixInfo{7, 8, InfixKind::Binary, BinaryOp::Div};
        case TokenTag::Percent:
            return InfixInfo{7, 8, InfixKind::Binary, BinaryOp::Rem};
        default:
            return std::nullopt;
    }
}

constexpr uint8_t PREFIX_BP = 8;

std::variant<Expr, ParseError> Parser::parse_expr(uint8_t min_bp) {
    auto left_res = parse_prefix();
    if (std::holds_alternative<ParseError>(left_res)) return left_res;
    Expr left = std::get<Expr>(std::move(left_res));

    while (true) {
        auto info = infix_binding(peek().kind.tag);
        if (!info) break;
        if (info->left_bp < min_bp) break;

        advance(); // consume operator

        if (info->kind == InfixKind::Assign) {
            if (!std::holds_alternative<ExprIdent>(left.node)) {
                return ParseError("invalid assignment target", left.span(), source_,
                                  std::make_optional("identifier"), std::nullopt);
            }
            std::string name = std::get<ExprIdent>(left.node).name;
            Span name_span = left.span();

            auto val_res = parse_expr(info->right_bp);
            if (std::holds_alternative<ParseError>(val_res)) return val_res;
            Expr value = std::get<Expr>(std::move(val_res));

            Span sp(name_span.start, value.span().end);
            left = Expr::make_assign(std::move(name), std::move(value), sp);
            continue;
        }

        auto right_res = parse_expr(info->right_bp);
        if (std::holds_alternative<ParseError>(right_res)) return right_res;
        Expr right = std::get<Expr>(std::move(right_res));

        Span sp(left.span().start, right.span().end);
        left = Expr::make_binary(info->op, std::move(left), std::move(right), sp);
    }

    return left;
}

std::variant<Expr, ParseError> Parser::parse_prefix() {
    Token tok = peek();
    if (tok.kind.tag == TokenTag::Minus) {
        advance();
        auto expr_res = parse_expr(PREFIX_BP);
        if (std::holds_alternative<ParseError>(expr_res)) return expr_res;
        Expr expr = std::get<Expr>(std::move(expr_res));
        Span sp(tok.span.start, expr.span().end);
        return Expr::make_unary(UnaryOp::Neg, std::move(expr), sp);
    }
    if (tok.kind.tag == TokenTag::Bang) {
        advance();
        auto expr_res = parse_expr(PREFIX_BP);
        if (std::holds_alternative<ParseError>(expr_res)) return expr_res;
        Expr expr = std::get<Expr>(std::move(expr_res));
        Span sp(tok.span.start, expr.span().end);
        return Expr::make_unary(UnaryOp::Not, std::move(expr), sp);
    }
    return parse_call_or_primary();
}

std::variant<Expr, ParseError> Parser::parse_call_or_primary() {
    auto prim_res = parse_primary();
    if (std::holds_alternative<ParseError>(prim_res)) return prim_res;
    Expr expr = std::get<Expr>(std::move(prim_res));

    if (!check(TokenTag::LParen)) {
        return expr;
    }

    if (!std::holds_alternative<ExprIdent>(expr.node)) {
        return ParseError("only identifiers can be called", expr.span(), source_);
    }

    std::string callee = std::get<ExprIdent>(expr.node).name;
    Span name_span = expr.span();

    advance(); // (
    std::vector<Expr> args;
    if (!check(TokenTag::RParen)) {
        while (true) {
            auto arg_res = parse_expr(0);
            if (std::holds_alternative<ParseError>(arg_res)) return arg_res;
            args.push_back(std::get<Expr>(std::move(arg_res)));
            if (!match_kind(TokenTag::Comma)) {
                break;
            }
        }
    }

    auto exp_rp = expect_kind(TokenTag::RParen, "`)`");
    if (std::holds_alternative<ParseError>(exp_rp)) return std::get<ParseError>(exp_rp);
    Token rp = std::get<Token>(exp_rp);

    Span sp(name_span.start, rp.span.end);
    return Expr::make_call(std::move(callee), std::move(args), sp);
}

std::variant<Expr, ParseError> Parser::parse_primary() {
    Token tok = peek();
    switch (tok.kind.tag) {
        case TokenTag::IntLit: {
            advance();
            return Expr::make_int(tok.kind.as_int(), tok.span);
        }
        case TokenTag::StringLit: {
            advance();
            return Expr::make_string(tok.kind.as_string(), tok.span);
        }
        case TokenTag::True: {
            advance();
            return Expr::make_bool(true, tok.span);
        }
        case TokenTag::False: {
            advance();
            return Expr::make_bool(false, tok.span);
        }
        case TokenTag::Ident: {
            advance();
            return Expr::make_ident(tok.kind.as_string(), tok.span);
        }
        case TokenTag::LParen: {
            advance();
            auto expr_res = parse_expr(0);
            if (std::holds_alternative<ParseError>(expr_res)) return expr_res;
            Expr expr = std::get<Expr>(std::move(expr_res));
            auto exp_rp = expect_kind(TokenTag::RParen, "`)`");
            if (std::holds_alternative<ParseError>(exp_rp)) return std::get<ParseError>(exp_rp);
            return expr;
        }
        default:
            return ParseError::unexpected(tok.span, source_, "expression", tok.kind);
    }
}

std::variant<Program, ParseError> parse(std::string_view source) {
    auto tok_res = tokenize(source);
    if (std::holds_alternative<LexError>(tok_res)) {
        return ParseError::from_lex(std::get<LexError>(tok_res));
    }
    const auto& tokens = std::get<std::vector<Token>>(tok_res);
    if (tokens.empty()) {
        return ParseError("empty token stream", Span::empty(Position::start()), source);
    }
    return Parser(tokens, source).parse_program();
}

} // namespace tiny
