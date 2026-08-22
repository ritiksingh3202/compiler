#include "tiny/lexer.hpp"
#include "tiny/ast.hpp"
#include "tiny/parser.hpp"
#include "tiny/sema.hpp"
#include "tiny/interp.hpp"
#include "tiny/ir.hpp"
#include "tiny/codegen.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

using namespace tiny;

namespace fs = std::filesystem;

static const char* HELP = R"(
Tiny compiler — interpret or compile .tiny programs

USAGE:
  tiny run <file.tiny>
  tiny build <file.tiny> [-o <output>] [--opt] [--emit-ast] [--emit-ir]
  tiny --help

COMMANDS:
  run      Lex, parse, type-check, and interpret
  build    Compile to a native executable (Linux x86-64 via cc/gcc)

OPTIONS:
  -o <path>    Output binary path (default: input stem)
  --opt        Enable IR constant folding and DCE
  --emit-ast   Print the AST and exit
  --emit-ir    Print three-address IR and exit
)";

static void print_help() {
    std::cout << HELP;
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("cannot read " + path + ": No such file or directory");
    }
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static Program compile_front(std::string_view source) {
    auto p_res = parse(source);
    if (std::holds_alternative<ParseError>(p_res)) {
        throw std::runtime_error(std::get<ParseError>(p_res).formatted);
    }
    const auto& program = std::get<Program>(p_res);
    auto sema_res = check(program, source);
    if (sema_res.has_value()) {
        throw std::runtime_error(sema_res->to_string());
    }
    return program;
}

static std::string default_output_name(const std::string& input) {
    fs::path p(input);
    if (p.has_stem()) {
        return p.stem().string();
    }
    return "a.out";
}

static void assemble_and_link(const std::string& asm_code, const std::string& output) {
    std::string asm_path = output + ".s";
    {
        std::ofstream out(asm_path);
        if (!out.is_open()) {
            throw std::runtime_error("write asm: failed to write to " + asm_path);
        }
        out << asm_code;
    }

    std::string cmd = "cc \"" + asm_path + "\" -o \"" + output + "\" -no-pie";
    int status = std::system(cmd.c_str());
    if (status != 0) {
        cmd = "gcc \"" + asm_path + "\" -o \"" + output + "\" -no-pie";
        status = std::system(cmd.c_str());
    }

    if (status != 0) {
        throw std::runtime_error("failed to invoke cc/gcc (need a C toolchain for linking)");
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty()) {
        print_help();
        return 0;
    }

    for (const auto& a : args) {
        if (a == "-h" || a == "--help") {
            print_help();
            return 0;
        }
    }

    std::string cmd = args[0];
    args.erase(args.begin());

    try {
        if (cmd == "run") {
            if (args.empty()) {
                throw std::runtime_error("usage: tiny run <file.tiny>");
            }
            std::string file = args[0];
            std::string source = read_file(file);
            Program program = compile_front(source);
            auto run_res = run(program, source, std::cout);
            if (std::holds_alternative<RuntimeError>(run_res)) {
                throw std::runtime_error(std::get<RuntimeError>(run_res).formatted);
            }
            return 0;
        } else if (cmd == "build") {
            std::optional<std::string> input;
            std::optional<std::string> output;
            bool emit_ast = false;
            bool emit_ir = false;
            bool opt = false;

            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i] == "-o") {
                    if (i + 1 < args.size()) {
                        output = args[++i];
                    }
                } else if (args[i] == "--emit-ast") {
                    emit_ast = true;
                } else if (args[i] == "--emit-ir") {
                    emit_ir = true;
                } else if (args[i] == "--opt") {
                    opt = true;
                } else if (!args[i].empty() && args[i][0] != '-') {
                    input = args[i];
                } else {
                    throw std::runtime_error("unknown option: " + args[i]);
                }
            }

            if (!input.has_value()) {
                throw std::runtime_error("usage: tiny build <file.tiny> [-o out] [--emit-ast] [--emit-ir] [--opt]");
            }

            std::string file = *input;
            std::string source = read_file(file);
            Program program = compile_front(source);

            if (emit_ast) {
                std::cout << "Program (functions=" << program.functions.size()
                          << ", statements=" << program.statements.size() << ")\n";
                return 0;
            }

            Module module = lower(program);
            if (opt) {
                optimize(module);
            }

            if (emit_ir) {
                std::cout << module.display();
                return 0;
            }

            std::string asm_code = emit_asm(module);
            std::string out_path = output.value_or(default_output_name(file));
            assemble_and_link(asm_code, out_path);
            return 0;
        } else {
            std::string err_msg = "unknown command `" + cmd + "`\n" + HELP;
            throw std::runtime_error(err_msg);
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
