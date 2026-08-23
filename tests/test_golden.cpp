#include "tiny/interp.hpp"
#include "tiny/parser.hpp"
#include "tiny/sema.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace tiny;
namespace fs = std::filesystem;

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

static std::string read_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        f.open("../" + path.string());
    }
    assert(f.is_open());
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static std::string interpret_file(const fs::path& path) {
    std::string src = read_file(path);
    auto p_res = parse(src);
    assert(std::holds_alternative<Program>(p_res));
    const auto& program = std::get<Program>(p_res);
    auto sema_res = check(program, src);
    assert(!sema_res.has_value());
    auto out_res = run_to_string(program, src);
    assert(std::holds_alternative<std::string>(out_res));
    return std::get<std::string>(out_res);
}

void test_golden_interpreter_matches_expected() {
    fs::path dir = "examples";
    if (!fs::exists(dir)) {
        dir = "../examples";
    }
    int count = 0;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        fs::path path = entry.path();
        if (path.extension() != ".tiny") continue;

        fs::path expected_path = path;
        expected_path.replace_extension(".expected");
        if (!fs::exists(expected_path)) continue;

        std::string expected = read_file(expected_path);
        std::string got = interpret_file(path);

        assert(normalize(got) == normalize(expected));
        std::cout << "  [PASS] " << path.filename().string() << std::endl;
        count++;
    }

    assert(count >= 3);
    std::cout << "Golden interpreter matched all " << count << " example files!" << std::endl;
}

int main() {
    test_golden_interpreter_matches_expected();
    std::cout << "All golden tests passed!" << std::endl;
    return 0;
}
