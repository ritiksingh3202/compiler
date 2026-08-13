#include "tiny/interp.hpp"
#include "tiny/parser.hpp"
#include "tiny/sema.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace tiny;

static std::string trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    return s;
}

static std::string normalize(std::string s) {
    std::string res;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') continue;
        res += s[i];
    }
    return trim(res);
}

static std::string interpret(std::string_view src) {
    auto p_res = parse(src);
    assert(std::holds_alternative<Program>(p_res));
    const auto& program = std::get<Program>(p_res);
    auto sema_res = check(program, src);
    assert(!sema_res.has_value());
    auto out_res = run_to_string(program, src);
    if (std::holds_alternative<RuntimeError>(out_res)) {
        std::cerr << "Runtime error: " << std::get<RuntimeError>(out_res).formatted << std::endl;
        assert(false);
    }
    return std::get<std::string>(out_res);
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        f.open("../" + path);
    }
    assert(f.is_open());
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void test_fib_output() {
    std::string src = read_file("examples/fib.tiny");
    assert(normalize(interpret(src)) == "55");
}

void test_loops_output() {
    std::string src = read_file("examples/loops.tiny");
    assert(normalize(interpret(src)) == "10");
}

void test_hello_output() {
    std::string src = read_file("examples/hello.tiny");
    assert(normalize(interpret(src)) == "true\nhello\n42");
}

void test_short_circuit_and() {
    const char* src = R"(
        fn boom() -> int { return 1 / 0; }
        print(false && boom() > 0);
    )";
    assert(normalize(interpret(src)) == "false");
}

void test_rec_while_example() {
    std::string src = read_file("examples/rec_while.tiny");
    assert(normalize(interpret(src)) == "15");
}

void test_nested_ret_example() {
    std::string src = read_file("examples/nested_ret.tiny");
    assert(normalize(interpret(src)) == "-1\n0\n1\n2");
}

void test_mixed_types_example() {
    std::string src = read_file("examples/mixed_types.tiny");
    assert(normalize(interpret(src)) == "tiny\n85\ntrue\nB\nC");
}

int main() {
    test_fib_output();
    test_loops_output();
    test_hello_output();
    test_short_circuit_and();
    test_rec_while_example();
    test_nested_ret_example();
    test_mixed_types_example();
    std::cout << "All interpreter tests passed (7/7)!" << std::endl;
    return 0;
}
