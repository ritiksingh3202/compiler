#include "tiny/sema.hpp"
#include "tiny/parser.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace tiny;

static void ok(std::string_view src) {
    auto p_res = parse(src);
    assert(std::holds_alternative<Program>(p_res));
    const auto& program = std::get<Program>(p_res);
    auto sema_res = check(program, src);
    if (sema_res.has_value()) {
        std::cerr << "Sema failed unexpectedly:\n" << sema_res->to_string() << std::endl;
        assert(false);
    }
}

static SemaErrors err(std::string_view src) {
    auto p_res = parse(src);
    assert(std::holds_alternative<Program>(p_res));
    const auto& program = std::get<Program>(p_res);
    auto sema_res = check(program, src);
    assert(sema_res.has_value());
    return *sema_res;
}

static std::pair<std::string, Position> first_msg(std::string_view src) {
    SemaErrors e = err(src);
    assert(!e.errors.empty());
    return {e.errors[0].message, e.errors[0].span.start};
}

void test_fib_passes() {
    std::ifstream f("examples/fib.tiny");
    if (!f.is_open()) {
        f.open("../examples/fib.tiny");
    }
    assert(f.is_open());
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ok(src);
}

void test_shadowing_in_nested_scope() {
    const char* src = R"(
        fn main() {
            let x: int = 1;
            {
                let x: int = 2;
                print(x);
            }
            print(x);
        }
    )";
    ok(src);
}

void test_use_before_declare() {
    auto [msg, pos] = first_msg("print(x); let x = 1;");
    assert(msg.find("undefined variable `x`") != std::string::npos);
    assert(pos == Position(1, 7));
}

void test_arithmetic_and_comparison_types() {
    ok("let a = 1 + 2 * 3; let b = a < 10; print(b);");
}

void test_logical_and_unary() {
    ok("let a = true && !false; let b = -1; print(a); print(b);");
}

void test_equality_on_string_and_bool() {
    ok("let s = \"hi\" == \"hi\"; let t = true != false; print(s);");
}

void test_let_with_and_without_annotation() {
    ok("let x: int = 1; let y = x; print(y);");
}

void test_let_annotation_mismatch() {
    auto [msg, pos] = first_msg("let x: bool = 1;");
    assert(msg.find("type mismatch") != std::string::npos);
    assert(pos.line == 1);
}

void test_call_arg_mismatch() {
    const char* src = R"(
        fn f(n: int) -> int { return n; }
        f(true);
    )";
    auto [msg, _] = first_msg(src);
    assert(msg.find("expected `int`") != std::string::npos || msg.find("argument") != std::string::npos);
}

void test_mutual_recursion_and_forward_ref() {
    const char* src = R"(
        fn a(n: int) -> int {
            if n < 1 { return 0; }
            return b(n - 1);
        }
        fn b(n: int) -> int {
            return a(n);
        }
        print(a(3));
    )";
    ok(src);
}

void test_missing_return_path() {
    const char* src = R"(
        fn f(n: int) -> int {
            if n < 0 { return 0; }
        }
    )";
    auto [msg, _] = first_msg(src);
    assert(msg.find("must return") != std::string::npos);
}

void test_both_branches_return_ok() {
    const char* src = R"(
        fn f(n: int) -> int {
            if n < 0 { return 0; } else { return 1; }
        }
        print(f(1));
    )";
    ok(src);
}

void test_void_function_and_return() {
    const char* src = R"(
        fn log(n: int) {
            print(n);
            return;
        }
        log(1);
    )";
    ok(src);
}

void test_return_value_in_void_fn() {
    const char* src = R"(
        fn f() {
            return 1;
        }
    )";
    auto [msg, _] = first_msg(src);
    assert(msg.find("void") != std::string::npos);
}

void test_print_each_type() {
    ok("print(1); print(true); print(\"hi\");");
}

void test_print_wrong_arity() {
    auto [msg, pos] = first_msg("print(1, 2);");
    assert(msg.find("print") != std::string::npos);
    assert(pos == Position(1, 1));
}

void test_undefined_function() {
    auto [msg, pos] = first_msg("foo(1);");
    assert(msg.find("undefined function `foo`") != std::string::npos);
    assert(pos == Position(1, 1));
}

void test_assign_type_mismatch() {
    auto [msg, _] = first_msg("let x: int = 1; x = true;");
    assert(msg.find("cannot assign") != std::string::npos);
}

void test_if_condition_not_bool() {
    auto [msg, _] = first_msg("if 1 { print(1); }");
    assert(msg.find("bool") != std::string::npos);
}

void test_error_has_caret() {
    SemaErrors e = err("let x: int = true;");
    assert(e.errors[0].formatted.find('^') != std::string::npos);
    assert(e.errors[0].formatted.find("-->") != std::string::npos);
}

void test_void_return_type_is_void() {
    const char* src = "fn f() { return; }";
    auto p_res = parse(src);
    assert(std::holds_alternative<Program>(p_res));
    const auto& program = std::get<Program>(p_res);
    assert(program.functions[0].return_type == Type::Void());
    ok(src);
}

int main() {
    test_fib_passes();
    test_shadowing_in_nested_scope();
    test_use_before_declare();
    test_arithmetic_and_comparison_types();
    test_logical_and_unary();
    test_equality_on_string_and_bool();
    test_let_with_and_without_annotation();
    test_let_annotation_mismatch();
    test_call_arg_mismatch();
    test_mutual_recursion_and_forward_ref();
    test_missing_return_path();
    test_both_branches_return_ok();
    test_void_function_and_return();
    test_return_value_in_void_fn();
    test_print_each_type();
    test_print_wrong_arity();
    test_undefined_function();
    test_assign_type_mismatch();
    test_if_condition_not_bool();
    test_error_has_caret();
    test_void_return_type_is_void();
    std::cout << "All sema tests passed (21/21)!" << std::endl;
    return 0;
}
