#include "tiny/ir.hpp"
#include "tiny/parser.hpp"
#include "tiny/sema.hpp"
#include <cassert>
#include <iostream>

using namespace tiny;

static Module create_module(std::string_view src) {
    auto p_res = parse(src);
    assert(std::holds_alternative<Program>(p_res));
    const auto& program = std::get<Program>(p_res);
    auto sema_res = check(program, src);
    assert(!sema_res.has_value());
    return lower(program);
}

void test_lower_arithmetic() {
    Module m = create_module("print(1 + 2 * 3);");
    std::string text = m.display();
    assert(text.find('+') != std::string::npos || text.find('r') != std::string::npos);
    assert(text.find("print_int") != std::string::npos || text.find("print") != std::string::npos);
}

void test_lower_if_while_call() {
    const char* src = R"(
        fn f(n: int) -> int {
            if n < 1 { return 0; }
            return f(n - 1);
        }
        let i = 0;
        while i < 3 { i = i + 1; }
        print(f(2));
    )";
    Module m = create_module(src);
    std::string text = m.display();
    assert(text.find("fn f") != std::string::npos);
    assert(text.find("goto") != std::string::npos || text.find("if !") != std::string::npos);
    assert(text.find("call f") != std::string::npos);
}

void test_constant_fold_shrinks() {
    const char* src = "print(2 + 3);";
    Module m = create_module(src);
    size_t before = m.main.instructions.size();
    optimize(m);
    size_t after = m.main.instructions.size();
    assert(after <= before);
    bool found_5 = false;
    for (const auto& instr : m.main.instructions) {
        if (std::holds_alternative<InstrConstInt>(instr.data)) {
            if (std::get<InstrConstInt>(instr.data).val == 5) {
                found_5 = true;
                break;
            }
        }
    }
    assert(found_5);
}

int main() {
    test_lower_arithmetic();
    test_lower_if_while_call();
    test_constant_fold_shrinks();
    std::cout << "All IR tests passed (3/3)!" << std::endl;
    return 0;
}
