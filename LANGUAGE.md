# Tiny Language Reference

Tiny is a small statically typed language that compiles to native x86-64 Linux code (and can be interpreted for reference semantics).

## Types

| Type | Literals | Notes |
|------|----------|--------|
| `int` | `0`, `42`, `-1` | 64-bit signed |
| `bool` | `true`, `false` | |
| `string` | `"hello"`, escapes `\n \t \r \\ \"` | |
| `void` | — | Default return type when `->` is omitted; also the type of `print` |

## Grammar (simplified)

```
program     ::= (fn_decl | stmt)*
fn_decl     ::= "fn" Ident "(" params? ")" ("->" type)? block
params      ::= param ("," param)*
param       ::= Ident ":" type
type        ::= "int" | "bool" | "string"

stmt        ::= let_stmt | return_stmt | if_stmt | while_stmt | block | expr_stmt
let_stmt    ::= "let" Ident (":" type)? "=" expr ";"
return_stmt ::= "return" expr? ";"
if_stmt     ::= "if" expr block ("else" (if_stmt | block))?
while_stmt  ::= "while" expr block
expr_stmt   ::= expr ";"
block       ::= "{" stmt* "}"

expr        ::= assignment
assignment  ::= Ident "=" assignment | logic_or
logic_or    ::= logic_and ("||" logic_and)*
logic_and   ::= equality ("&&" equality)*
equality    ::= comparison (("==" | "!=") comparison)*
comparison  ::= term (("<" | "<=" | ">" | ">=") term)*
term        ::= factor (("+" | "-") factor)*
factor       ::= unary (("*" | "/" | "%") unary)*
unary       ::= ("!" | "-") unary | call
call        ::= primary ("(" args? ")")?
primary     ::= IntLit | StringLit | "true" | "false" | Ident | "(" expr ")"
```

Semicolons are required after `let`, `return`, and expression statements. They are not used after `if` / `while` / `fn` / bare blocks.

## Semantics

- **Scopes**: function bodies, `if`/`else`/`while` bodies, and bare blocks introduce scopes. Nested scopes may shadow outer names.
- **Functions**: all signatures are registered before bodies are checked (forward references and mutual recursion work).
- **Returns**: non-`void` functions must return on every path through `if`/`else` (loops are not treated as always-returning).
- **`print(x)`**: builtin; one argument of type `int`, `bool`, or `string`; returns `void`.
- **Operators**: arithmetic/`%` on `int`; comparisons on `int`; `==`/`!=` on `int`/`bool`/`string`; `&&`/`||`/`!` on `bool` with short-circuit evaluation.

## Pipeline

```
source → lexer → parser → sema → (interpreter)
                           ↓
                          IR → opt? → x86-64 asm → cc/gcc → executable
```
