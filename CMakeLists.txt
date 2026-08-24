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
)

add_library(tiny_core STATIC ${TINY_SOURCES})
target_include_directories(tiny_core PUBLIC include)

add_executable(tiny src/cli/main.cpp)
target_link_libraries(tiny PRIVATE tiny_core)

enable_testing()

function(add_tiny_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE tiny_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_tiny_test(test_lexer tests/test_lexer.cpp)
add_tiny_test(test_parser tests/test_parser.cpp)
add_tiny_test(test_sema tests/test_sema.cpp)
add_tiny_test(test_interp tests/test_interp.cpp)
add_tiny_test(test_ir tests/test_ir.cpp)
add_tiny_test(test_golden tests/test_golden.cpp)
