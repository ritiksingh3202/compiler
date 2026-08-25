# Tiny

[![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Standard](https://img.shields.io/badge/standard-ISO%20C%2B%2B20-00599C.svg)](https://en.cppreference.com)
[![Build](https://img.shields.io/badge/build-CMake-brightgreen.svg)](https://cmake.org)
[![Tests](https://img.shields.io/badge/tests-59%20passed-success.svg)](#test-suite)

**Tiny** is a small, statically typed programming language with a complete, end-to-end optimizing compiler implemented from scratch in **modern C++ (C++20)**. 

The project contains **zero third-party compiler frameworks or parser generators** (no LLVM, Flex, Bison, or ANTLR). Every stage of the compiler pipeline—from character-by-character tokenization to x86-64 machine code generation—is built by hand using modern C++ design principles (RAII, `std::variant`, `std::unique_ptr`, `std::optional`, and value semantics).

---

## Architecture & Pipeline

The compiler follows a clean multi-tier pipeline with dual backends (a tree-walking reference interpreter and an x86-64 native code generator):

```
                                      ┌─────────────────────────────────┐
                                      │       Tiny Source Code          │
                                      └────────────────┬────────────────┘
                                                       │
                                                       ▼
                                      ┌─────────────────────────────────┐
                                      │      Hand-written Lexer         │ (Stream of Tokens + Spans)
                                      └────────────────┬────────────────┘
                                                       │
                                                       ▼
                                      ┌─────────────────────────────────┐
                                      │    Recursive Descent / Pratt    │ (Abstract Syntax Tree)
                                      └────────────────┬────────────────┘
                                                       │
                                                       ▼
                                      ┌─────────────────────────────────┐
                                      │   Semantic Analysis & Typeck    │ (Symbol Tables & Type Resolution)
                                      └───────┬─────────────────┬───────┘
                                              │                 │
                         Reference Semantics  │                 │ Compilation
                                              ▼                 ▼
                          ┌─────────────────────┐   ┌─────────────────────────┐
                          │ Tree-Walk Evaluator │   │     AST Lowering Pass   │
                          └─────────────────────┘   └───────────┬─────────────┘
                                                                │
                                                                ▼
                                                    ┌─────────────────────────┐
                                                    │ Three-Address Code (IR) │
                                                    └───────────┬─────────────┘
                                                                │
                                                                ▼
                                                    ┌─────────────────────────┐
                                                    │ Optimizations: CF & DCE │ (Constant Folding & Dead Code)
                                                    └───────────┬─────────────┘
                                                                │
                                                                ▼
                                                    ┌─────────────────────────┐
                                                    │  x86-64 Code Generator  │ (AT&T Assembly, System V ABI)
                                                    └───────────┬─────────────┘
                                                                │
                                                                ▼
                                                    ┌─────────────────────────┐
                                                    │    System Assembler     │ (cc / gcc)
                                                    └───────────┬─────────────┘
                                                                │
                                                                ▼
                                                    ┌─────────────────────────┐
                                                    │    Native Executable    │
                                                    └─────────────────────────┘
```

---

## Compiler Stages Overview

1. **Lexical Analysis (`src/lexer/`, `include/tiny/lexer.hpp`)**
   - Character scanner tracking byte-accurate source spans (`line:col` locations).
   - Recognizes keywords (`fn`, `let`, `if`, `else`, `while`, `return`, `int`, `bool`, `string`), multi-character operators (`==`, `!=`, `<=`, `>=`, `&&`, `||`), identifiers, and escaped string literals.

2. **Parsing & AST Construction (`src/parser/`, `src/ast/`)**
   - Hybrid recursive-descent statement parser with Pratt operator precedence for expressions.
   - Distinct precedence binding for arithmetic, comparisons, equality, and short-circuit boolean logic.
   - Clear AST representation utilizing polymorphic expression nodes and statement variants.

3. **Semantic Analysis & Type Checking (`src/sema/`)**
   - Two-pass validation: pass 1 gathers top-level function signatures for forward references and recursion; pass 2 checks expressions, variable bindings, and function bodies.
   - Lexical scoping with shadowing support and detection of undeclared/re-declared identifiers.
   - Return-path validation guaranteeing that non-void functions return a value along every execution path.
   - Clear diagnostics with caret pointers displaying source code context.

4. **Reference Interpreter (`src/interpreter/`)**
   - Tree-walking evaluator operating directly on the verified AST.
   - Serves as the ground truth reference for runtime semantics and differential validation.

5. **Intermediate Representation & Optimization (`src/ir/`)**
   - Three-Address Code (TAC) linear representation with explicit basic blocks, jumps, labels, and temporaries.
   - Constant folding pass: statically computes compile-time binary/unary operations.
   - Dead-code elimination (DCE): eliminates unreachable basic blocks following unconditional jumps or returns.

6. **Native x86-64 Assembly Generator (`src/codegen/`)**
   - Generates GNU AT&T syntax assembly compliant with the **System V AMD64 ABI**.
   - Handles stack alignment, argument passing, caller/callee-saved registers, function prologues/epilogues, and standard runtime library linking (`printf`/`puts`).

---

## Language at a Glance

```rust
// Recursive Fibonacci
fn fib(n: int) -> int {
    if n <= 1 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    let count: int = 10;
    let result: int = fib(count);
    print(result); // Built-in print supports int, bool, string
}
```

Detailed grammar specifications and semantics are documented in [LANGUAGE.md](LANGUAGE.md).

---

## Requirements

- **C++ Compiler**: C++20 compliant compiler (`clang++` >= 14 or `g++` >= 11)
- **Build System**: `cmake` >= 3.20 and build tools (`make`, `ninja`, or MinGW)
- **Native Target**: Linux x86-64 (or WSL on Windows) with `cc` or `gcc` installed for assembling and linking. The interpreter is completely cross-platform and runs natively on Windows, macOS, and Linux.

---

## Building and Running

### 1. Build the Project
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build produces:
- `build/tiny`: The compiler CLI driver.
- `build/libtiny_core.a`: The static compiler library.
- `build/test_*`: Unit and golden test executables.

### 2. Run Tests
```bash
ctest --test-dir build --output-on-failure
```

### 3. Run Programs Directly (Interpreter)
```bash
./build/tiny run examples/fib.tiny
# Output: 55
```

### 4. Compile to a Native Executable (x86-64 Linux)
```bash
./build/tiny build examples/fib.tiny -o fib
./fib
# Output: 55
```

### 5. Inspect Intermediate Pipeline Stages
```bash
# Print parsed AST structure
./build/tiny build examples/fib.tiny --emit-ast

# Print three-address intermediate representation
./build/tiny build examples/fib.tiny --emit-ir

# Enable IR optimizations (constant folding and dead-code elimination)
./build/tiny build examples/fib.tiny --opt -o fib
```
