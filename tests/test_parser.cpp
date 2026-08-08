#include "tiny/parser.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace tiny;

static Span empty_span() {
    return Span::empty(Position::start());
}

static Expr strip_expr(const Expr& expr);

static Expr strip_expr(const Expr& expr) {
    return std::visit([&](const auto& val) -> Expr {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, ExprIntLit>) {
            return Expr::make_int(val.value, empty_span());
        } else if constexpr (std::is_same_v<T, ExprStringLit>) {
            return Expr::make_string(val.value, empty_span());
        } else if constexpr (std::is_same_v<T, ExprBoolLit>) {
            return Expr::make_bool(val.value, empty_span());
        } else if constexpr (std::is_same_v<T, ExprIdent>) {
            return Expr::make_ident(val.name, empty_span());
        } else if constexpr (std::is_same_v<T, ExprUnary>) {
            return Expr::make_unary(val.op, strip_expr(*val.expr), empty_span());
        } else if constexpr (std::is_same_v<T, ExprBinary>) {
            return Expr::make_binary(val.op, strip_expr(*val.left), strip_expr(*val.right), empty_span());
        } else if constexpr (std::is_same_v<T, ExprAssign>) {
            return Expr::make_assign(val.name, strip_expr(*val.value), empty_span());
        } else if constexpr (std::is_same_v<T, ExprCall>) {
            std::vector<Expr> args;
            for (const auto& a : val.args) {
                args.push_back(strip_expr(a));
            }
            return Expr::make_call(val.callee, std::move(args), empty_span());
        }
    }, expr.node);
}

static Block strip_block(const Block& block);

static Stmt strip_stmt(const Stmt& stmt) {
    return std::visit([&](const auto& val) -> Stmt {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, StmtLet>) {
            return Stmt::make_let(val.name, val.ty, strip_expr(val.init), empty_span());
        } else if constexpr (std::is_same_v<T, StmtExpr>) {
            return Stmt::make_expr(strip_expr(val.expr));
        } else if constexpr (std::is_same_v<T, StmtReturn>) {
            std::optional<Expr> v = std::nullopt;
            if (val.value) {
                v = strip_expr(*val.value);
            }
            return Stmt::make_return(std::move(v), empty_span());
        } else if constexpr (std::is_same_v<T, StmtIf>) {
            std::optional<Block> else_b = std::nullopt;
            if (val.else_block) {
                else_b = strip_block(*val.else_block);
            }
            return Stmt::make_if(strip_expr(val.cond), strip_block(val.then_block), std::move(else_b), empty_span());
        } else if constexpr (std::is_same_v<T, StmtWhile>) {
            return Stmt::make_while(strip_expr(val.cond), strip_block(val.body), empty_span());
        } else if constexpr (std::is_same_v<T, StmtBlock>) {
            return Stmt::make_block(strip_block(val.block));
        }
    }, stmt.node);
}

static Block strip_block(const Block& block) {
    std::vector<Stmt> stmts;
    for (const auto& s : block.statements) {
        stmts.push_back(strip_stmt(s));
    }
    return Block{std::move(stmts), empty_span()};
}

static Program strip_program(const Program& program) {
    std::vector<FnDecl> fns;
    for (const auto& f : program.functions) {
        std::vector<Param> params;
        for (const auto& p : f.params) {
            params.push_back(Param{p.name, p.ty, empty_span()});
        }
        fns.push_back(FnDecl{f.name, std::move(params), f.return_type, strip_block(f.body), empty_span()});
    }
    std::vector<Stmt> stmts;
    for (const auto& s : program.statements) {
        stmts.push_back(strip_stmt(s));
    }
    return Program{std::move(fns), std::move(stmts), empty_span()};
}

static Expr parse_expr_str(std::string_view src) {
    std::string s = std::string(src) + ";";
    auto res = parse(s);
    assert(std::holds_alternative<Program>(res));
    const auto& prog = std::get<Program>(res);
    assert(!prog.statements.empty());
    assert(std::holds_alternative<StmtExpr>(prog.statements[0].node));
    return strip_expr(std::get<StmtExpr>(prog.statements[0].node).expr);
}

static Expr int_expr(int64_t n) {
    return Expr::make_int(n, empty_span());
}

static Expr ident_expr(std::string name) {
    return Expr::make_ident(std::move(name), empty_span());
}

static Expr bin_expr(BinaryOp op, Expr left, Expr right) {
    return Expr::make_binary(op, std::move(left), std::move(right), empty_span());
}

static Expr unary_expr(UnaryOp op, Expr expr) {
    return Expr::make_unary(op, std::move(expr), empty_span());
}

void test_arithmetic_precedence() {
    assert(parse_expr_str("1 + 2 * 3") ==
           bin_expr(BinaryOp::Add, int_expr(1), bin_expr(BinaryOp::Mul, int_expr(2), int_expr(3))));
}

void test_arithmetic_left_associativity() {
    assert(parse_expr_str("1 - 2 - 3") ==
           bin_expr(BinaryOp::Sub, bin_expr(BinaryOp::Sub, int_expr(1), int_expr(2)), int_expr(3)));
}

void test_unary_vs_binary_minus() {
    assert(parse_expr_str("-1 + 2") ==
           bin_expr(BinaryOp::Add, unary_expr(UnaryOp::Neg, int_expr(1)), int_expr(2)));
    assert(parse_expr_str("1 + -2") ==
           bin_expr(BinaryOp::Add, int_expr(1), unary_expr(UnaryOp::Neg, int_expr(2))));
}

void test_comparison_and_equality() {
    assert(parse_expr_str("a < b == c") ==
           bin_expr(BinaryOp::Eq, bin_expr(BinaryOp::Lt, ident_expr("a"), ident_expr("b")), ident_expr("c")));
}

void test_logical_operator_precedence() {
    assert(parse_expr_str("a || b && c") ==
           bin_expr(BinaryOp::Or, ident_expr("a"), bin_expr(BinaryOp::And, ident_expr("b"), ident_expr("c"))));
    assert(parse_expr_str("!a && b") ==
           bin_expr(BinaryOp::And, unary_expr(UnaryOp::Not, ident_expr("a")), ident_expr("b")));
}

void test_assignment_is_right_associative() {
    Expr expected = Expr::make_assign("a", Expr::make_assign("b", int_expr(1), empty_span()), empty_span());
    assert(parse_expr_str("a = b = 1") == expected);
}

void test_if_else_and_else_if() {
    const char* src = R"(
        if x {
            return 1;
        } else if y {
            return 2;
        } else {
            return 3;
        }
    )";
    auto res = parse(src);
    assert(std::holds_alternative<Program>(res));
    Program program = strip_program(std::get<Program>(res));
    assert(program.statements.size() == 1);
    assert(std::holds_alternative<StmtIf>(program.statements[0].node));
    const auto& if_stmt = std::get<StmtIf>(program.statements[0].node);
    assert(if_stmt.else_block.has_value());
    assert(if_stmt.else_block->statements.size() == 1);
    assert(std::holds_alternative<StmtIf>(if_stmt.else_block->statements[0].node));
}

void test_while_loop() {
    const char* src = "while n > 0 { n = n - 1; }";
    auto res = parse(src);
    assert(std::holds_alternative<Program>(res));
    Program program = strip_program(std::get<Program>(res));

    std::vector<Stmt> expected_body;
    expected_body.push_back(Stmt::make_expr(Expr::make_assign("n", bin_expr(BinaryOp::Sub, ident_expr("n"), int_expr(1)), empty_span())));
    Block body{std::move(expected_body), empty_span()};

    std::vector<Stmt> expected_stmts;
    expected_stmts.push_back(Stmt::make_while(bin_expr(BinaryOp::Gt, ident_expr("n"), int_expr(0)), std::move(body), empty_span()));

    assert(program.statements == expected_stmts);
}

void test_function_with_typed_params() {
    const char* src = R"(
        fn add(a: int, b: int) -> int {
            return a + b;
        }
    )";
    auto res = parse(src);
    assert(std::holds_alternative<Program>(res));
    Program program = strip_program(std::get<Program>(res));

    std::vector<Param> params = {
        Param{"a", Type::Int(), empty_span()},
        Param{"b", Type::Int(), empty_span()}
    };
    std::vector<Stmt> body_stmts;
    body_stmts.push_back(Stmt::make_return(bin_expr(BinaryOp::Add, ident_expr("a"), ident_expr("b")), empty_span()));
    Block body{std::move(body_stmts), empty_span()};

    std::vector<FnDecl> fns;
    fns.push_back(FnDecl{"add", std::move(params), Type::Int(), std::move(body), empty_span()});

    assert(program.functions == fns);
}

void test_nested_blocks_and_let() {
    const char* src = "{ let x: int = 1; { let y = x; } }";
    auto res = parse(src);
    assert(std::holds_alternative<Program>(res));
    Program program = strip_program(std::get<Program>(res));
    assert(program.statements.size() == 1);
    assert(std::holds_alternative<StmtBlock>(program.statements[0].node));
    const auto& outer = std::get<StmtBlock>(program.statements[0].node).block;
    assert(outer.statements.size() == 2);
    assert(std::holds_alternative<StmtLet>(outer.statements[0].node));
    const auto& let_s = std::get<StmtLet>(outer.statements[0].node);
    assert(let_s.name == "x");
    assert(let_s.ty == Type::Int());
    assert(std::holds_alternative<StmtBlock>(outer.statements[1].node));
}

void test_missing_semicolon_reports_span() {
    auto res = parse("let x = 1");
    assert(std::holds_alternative<ParseError>(res));
    const auto& err = std::get<ParseError>(res);
    assert(err.message.find("`;`") != std::string::npos || err.formatted.find("`;`") != std::string::npos);
    assert(err.span.start.line == 1);
    assert(err.formatted.find('^') != std::string::npos);
}

void test_missing_paren_in_call() {
    auto res = parse("print(1;");
    assert(std::holds_alternative<ParseError>(res));
    const auto& err = std::get<ParseError>(res);
    assert(err.formatted.find('^') != std::string::npos);
    assert(err.expected.has_value() || err.message.find(')') != std::string::npos);
}

void test_invalid_assignment_target() {
    auto res = parse("1 = 2;");
    assert(std::holds_alternative<ParseError>(res));
    const auto& err = std::get<ParseError>(res);
    assert(err.message.find("assignment") != std::string::npos);
    assert(err.span.start == Position(1, 1));
    assert(err.formatted.find('^') != std::string::npos);
}

void test_fib_example_parses() {
    std::ifstream f("examples/fib.tiny");
    if (!f.is_open()) {
        f.open("../examples/fib.tiny");
    }
    assert(f.is_open());
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto res = parse(src);
    assert(std::holds_alternative<Program>(res));
    const auto& program = std::get<Program>(res);
    assert(program.functions.size() == 1);
    assert(program.functions[0].name == "fib");
    assert(program.functions[0].params.size() == 1);
    assert(program.functions[0].return_type == Type::Int());
    assert(program.statements.size() == 1);
    assert(std::holds_alternative<StmtExpr>(program.statements[0].node));
    const auto& expr = std::get<StmtExpr>(program.statements[0].node).expr;
    assert(std::holds_alternative<ExprCall>(expr.node));
    const auto& call = std::get<ExprCall>(expr.node);
    assert(call.callee == "print");
    assert(call.args.size() == 1);
}

int main() {
    test_arithmetic_precedence();
    test_arithmetic_left_associativity();
    test_unary_vs_binary_minus();
    test_comparison_and_equality();
    test_logical_operator_precedence();
    test_assignment_is_right_associative();
    test_if_else_and_else_if();
    test_while_loop();
    test_function_with_typed_params();
    test_nested_blocks_and_let();
    test_missing_semicolon_reports_span();
    test_missing_paren_in_call();
    test_invalid_assignment_target();
    test_fib_example_parses();
    std::cout << "All parser tests passed (14/14)!" << std::endl;
    return 0;
}
