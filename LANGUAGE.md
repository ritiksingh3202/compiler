# Tiny Language Reference

Tiny is a small statically typed language that compiles to native x86-64 Linux code (and can be interpreted for reference semantics).

## Types

| Type | Literals | Notes |
|------|----------|--------|
| `int` | `0`, `42`, `-1` | 64-bit signed |
| `bool` | `true`, `false` | |
| `string` | `"hello"`, escapes `\n \t \r \\ \"` | |
| `void` | — | Default return type when `->` is omitted; also the type of `print` |

