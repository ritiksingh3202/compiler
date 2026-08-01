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
