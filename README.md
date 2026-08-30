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

## Examples

| Program | Output |
|---------|--------|
| `examples/fib.tiny` | `55` |
| `examples/loops.tiny` | `10` |
| `examples/hello.tiny` | `true` / `hello` / `42` |
| `examples/rec_while.tiny` | `15` |
| `examples/nested_ret.tiny` | `-1` / `0` / `1` / `2` |
| `examples/mixed_types.tiny` | `tiny` / `85` / `true` / `B` / `C` |

## Crates

| Crate | Role |
|-------|------|
| `tiny-lexer` | Tokens, spans, diagnostics |
| `tiny-ast` | AST types |
| `tiny-parser` | Recursive descent + Pratt |
| `tiny-sema` | Scopes & type checking |
| `tiny-interp` | Reference interpreter |
| `tiny-ir` | TAC IR, lowering, opts |
| `tiny-codegen` | x86-64 AT&T assembly |
| `tiny-cli` | `tiny` binary |

See [LANGUAGE.md](LANGUAGE.md) for grammar and semantics.

