# Tiny

A small statically typed language with a from-scratch Rust compiler: hand-written lexer & parser, type checker, tree-walking interpreter, three-address IR, and x86-64 Linux code generation.

```
source → lex → parse → sema → IR → opt? → asm → cc → binary
                      ↘ interpreter (reference semantics)
```

