# Tiny

A small statically typed language with a from-scratch Rust compiler: hand-written lexer & parser, type checker, tree-walking interpreter, three-address IR, and x86-64 Linux code generation.

```
source → lex → parse → sema → IR → opt? → asm → cc → binary
                      ↘ interpreter (reference semantics)
```

## Build

Requires Rust (stable). Native binaries need a Linux x86-64 host (or WSL) with `gcc`/`cc`.

```bash
cargo build --release
cargo install --path crates/tiny-cli
```

## Usage

```bash
# Interpret (works on any OS)
tiny run examples/fib.tiny          # prints 55

# Compile to native executable (Linux)
tiny build examples/fib.tiny -o fib
./fib                               # 55

# Inspect pipeline stages
tiny build examples/fib.tiny --emit-ast
tiny build examples/fib.tiny --emit-ir
tiny build examples/fib.tiny --opt -o fib
```

