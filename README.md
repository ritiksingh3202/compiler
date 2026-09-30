# Tiny

Tiny is a small, statically typed language with a compiler written from scratch in Rust. It has a hand-written lexer and parser, a type checker, a tree-walking interpreter, a three-address-code IR, and an x86-64 Linux backend that emits assembly and hands it to the system C compiler for assembling and linking.

The project has no parser generators or codegen frameworks. Every stage of the pipeline is implemented in this repository.

```
source → lex → parse → sema → IR → opt (optional) → asm → cc → binary
                          ↘
                           interpreter (reference semantics)
```

The interpreter runs directly off the checked AST and defines what a correct program means. The native backend is tested against it.

## Requirements

- Rust (stable toolchain)
- For native compilation: a Linux x86-64 host (WSL works) with `gcc` or `cc` on the path

The interpreter has no platform requirements beyond Rust.

## Building

```bash
cargo build --release
cargo install --path crates/tiny-cli
```

This installs the `tiny` binary.

## Usage

Run a program with the interpreter:

```bash
tiny run examples/fib.tiny
# 55
```

Compile to a native executable (Linux x86-64 only):

```bash
tiny build examples/fib.tiny -o fib
./fib
# 55
```

Inspect intermediate stages:

```bash
tiny build examples/fib.tiny --emit-ast    # parsed AST
tiny build examples/fib.tiny --emit-ir     # three-address IR
tiny build examples/fib.tiny --opt -o fib  # build with IR optimizations enabled
```

## Examples

The `examples/` directory doubles as the golden test corpus.

| File | Expected output |
|------|-----------------|
| `fib.tiny` | `55` |
| `loops.tiny` | `10` |
| `hello.tiny` | `true`, `hello`, `42` |
| `rec_while.tiny` | `15` |
| `nested_ret.tiny` | `-1`, `0`, `1`, `2` |
| `mixed_types.tiny` | `tiny`, `85`, `true`, `B`, `C` |

## Project layout

The compiler is a Cargo workspace with one crate per stage, so each can be read and tested in isolation.

| Crate | Responsibility |
|-------|----------------|
| `tiny-lexer` | Tokens, source spans, diagnostics |
| `tiny-ast` | AST definitions |
| `tiny-parser` | Recursive descent parser with Pratt-style expression parsing |
| `tiny-sema` | Scope resolution and type checking |
| `tiny-interp` | Tree-walking reference interpreter |
| `tiny-ir` | Three-address IR, AST lowering, optimization passes |
| `tiny-codegen` | x86-64 assembly generation (AT&T syntax) |
| `tiny-cli` | The `tiny` command-line driver |

## Testing

```bash
cargo test --workspace
```

On Linux, the golden tests compile every program in `examples/` to a native binary and assert that its stdout matches the interpreter's output. Elsewhere, only the interpreter-side tests run.

## Language reference

Grammar and semantics are documented in [LANGUAGE.md](LANGUAGE.md).
