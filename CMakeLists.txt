cmake_minimum_required(VERSION 3.20)
project(tiny_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

include_directories(include)

set(TINY_SOURCES
    src/lexer/lexer.cpp
    src/ast/ast.cpp
    src/parser/parser.cpp
    src/sema/typeck.cpp
    src/interpreter/eval.cpp
    src/ir/ir.cpp
    src/ir/lower.cpp
    src/ir/opt.cpp
    src/codegen/x86.cpp
