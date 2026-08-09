//! Unit tests for the Tiny parser (PartialEq AST assertions).

use tiny_ast::{
    BinaryOp, Block, Expr, FnDecl, Param, Program, Stmt, Type, UnaryOp,
};
use tiny_lexer::{Position, Span};
use tiny_parser::parse;

fn empty_span() -> Span {
    Span::empty(Position::start())
}

fn strip_expr(expr: Expr) -> Expr {
    match expr {
        Expr::IntLit { value, .. } => Expr::IntLit {
            value,
            span: empty_span(),
        },
        Expr::StringLit { value, .. } => Expr::StringLit {
            value,
            span: empty_span(),
        },
        Expr::BoolLit { value, .. } => Expr::BoolLit {
            value,
            span: empty_span(),
        },
        Expr::Ident { name, .. } => Expr::Ident {
            name,
            span: empty_span(),
        },
        Expr::Unary { op, expr, .. } => Expr::Unary {
            op,
            expr: Box::new(strip_expr(*expr)),
            span: empty_span(),
        },
        Expr::Binary {
            op, left, right, ..
        } => Expr::Binary {
            op,
            left: Box::new(strip_expr(*left)),
            right: Box::new(strip_expr(*right)),
            span: empty_span(),
        },
        Expr::Call { callee, args, .. } => Expr::Call {
            callee,
            args: args.into_iter().map(strip_expr).collect(),
            span: empty_span(),
        },
        Expr::Assign { name, value, .. } => Expr::Assign {
            name,
            value: Box::new(strip_expr(*value)),
            span: empty_span(),
        },
    }
}

fn strip_stmt(stmt: Stmt) -> Stmt {
    match stmt {
        Stmt::Let { name, ty, init, .. } => Stmt::Let {
            name,
            ty,
            init: strip_expr(init),
            span: empty_span(),
        },
        Stmt::Expr(e) => Stmt::Expr(strip_expr(e)),
        Stmt::Return { value, .. } => Stmt::Return {
            value: value.map(strip_expr),
            span: empty_span(),
        },
        Stmt::If {
            cond,
            then_block,
            else_block,
            ..
        } => Stmt::If {
            cond: strip_expr(cond),
            then_block: strip_block(then_block),
            else_block: else_block.map(strip_block),
            span: empty_span(),
        },
        Stmt::While { cond, body, .. } => Stmt::While {
            cond: strip_expr(cond),
            body: strip_block(body),
            span: empty_span(),
        },
        Stmt::Block(b) => Stmt::Block(strip_block(b)),
    }
}

fn strip_block(block: Block) -> Block {
    Block {
        statements: block.statements.into_iter().map(strip_stmt).collect(),
        span: empty_span(),
    }
}

fn strip_program(program: Program) -> Program {
    Program {
        functions: program
            .functions
            .into_iter()
            .map(|f| FnDecl {
                name: f.name,
                params: f
                    .params
                    .into_iter()
                    .map(|p| Param {
                        name: p.name,
                        ty: p.ty,
                        span: empty_span(),
                    })
                    .collect(),
                return_type: f.return_type,
                body: strip_block(f.body),
                span: empty_span(),
            })
            .collect(),
        statements: program.statements.into_iter().map(strip_stmt).collect(),
        span: empty_span(),
    }
}

fn parse_expr(src: &str) -> Expr {
    let program = parse(&format!("{src};")).expect("parse ok");
    match program.statements.into_iter().next() {
        Some(Stmt::Expr(e)) => strip_expr(e),
        other => panic!("expected expr stmt, got {other:?}"),
    }
}

fn int(n: i64) -> Expr {
    Expr::IntLit {
        value: n,
        span: empty_span(),
    }
}

fn ident(name: &str) -> Expr {
    Expr::Ident {
        name: name.into(),
        span: empty_span(),
    }
}

fn bin(op: BinaryOp, left: Expr, right: Expr) -> Expr {
    Expr::Binary {
        op,
        left: Box::new(left),
        right: Box::new(right),
        span: empty_span(),
    }
}

fn unary(op: UnaryOp, expr: Expr) -> Expr {
    Expr::Unary {
        op,
        expr: Box::new(expr),
        span: empty_span(),
    }
}

#[test]
fn arithmetic_precedence() {
    // 1 + 2 * 3  =>  1 + (2 * 3)
    assert_eq!(
        parse_expr("1 + 2 * 3"),
        bin(BinaryOp::Add, int(1), bin(BinaryOp::Mul, int(2), int(3)))
    );
}

#[test]
fn arithmetic_left_associativity() {
    // 1 - 2 - 3  =>  (1 - 2) - 3
    assert_eq!(
        parse_expr("1 - 2 - 3"),
        bin(BinaryOp::Sub, bin(BinaryOp::Sub, int(1), int(2)), int(3))
    );
}

#[test]
fn unary_vs_binary_minus() {
    assert_eq!(
        parse_expr("-1 + 2"),
        bin(BinaryOp::Add, unary(UnaryOp::Neg, int(1)), int(2))
    );
    assert_eq!(
        parse_expr("1 + -2"),
        bin(BinaryOp::Add, int(1), unary(UnaryOp::Neg, int(2)))
    );
}

#[test]
fn comparison_and_equality() {
    assert_eq!(
        parse_expr("a < b == c"),
        bin(
            BinaryOp::Eq,
            bin(BinaryOp::Lt, ident("a"), ident("b")),
            ident("c")
        )
    );
}

#[test]
fn logical_operator_precedence() {
    // a || b && c  =>  a || (b && c)
    assert_eq!(
        parse_expr("a || b && c"),
        bin(
            BinaryOp::Or,
            ident("a"),
            bin(BinaryOp::And, ident("b"), ident("c"))
        )
    );
    assert_eq!(
        parse_expr("!a && b"),
        bin(BinaryOp::And, unary(UnaryOp::Not, ident("a")), ident("b"))
    );
}

#[test]
fn assignment_is_right_associative() {
    assert_eq!(
        parse_expr("a = b = 1"),
        Expr::Assign {
            name: "a".into(),
            value: Box::new(Expr::Assign {
                name: "b".into(),
                value: Box::new(int(1)),
                span: empty_span(),
            }),
            span: empty_span(),
        }
    );
}

#[test]
fn if_else_and_else_if() {
    let src = r#"
        if x {
            return 1;
        } else if y {
            return 2;
        } else {
            return 3;
        }
    "#;
    let program = strip_program(parse(src).expect("parse ok"));
    assert_eq!(program.statements.len(), 1);
    match &program.statements[0] {
        Stmt::If {
            else_block: Some(else_b),
            ..
        } => {
            assert_eq!(else_b.statements.len(), 1);
            assert!(matches!(else_b.statements[0], Stmt::If { .. }));
        }
        other => panic!("expected if, got {other:?}"),
    }
}

#[test]
fn while_loop() {
    let src = "while n > 0 { n = n - 1; }";
    let program = strip_program(parse(src).expect("parse ok"));
    assert_eq!(
        program.statements,
        vec![Stmt::While {
            cond: bin(BinaryOp::Gt, ident("n"), int(0)),
            body: Block {
                statements: vec![Stmt::Expr(Expr::Assign {
                    name: "n".into(),
                    value: Box::new(bin(BinaryOp::Sub, ident("n"), int(1))),
                    span: empty_span(),
                })],
                span: empty_span(),
            },
            span: empty_span(),
        }]
    );
}

#[test]
fn function_with_typed_params() {
    let src = r#"
        fn add(a: int, b: int) -> int {
            return a + b;
        }
    "#;
    let program = strip_program(parse(src).expect("parse ok"));
    assert_eq!(
        program.functions,
        vec![FnDecl {
            name: "add".into(),
            params: vec![
                Param {
                    name: "a".into(),
                    ty: Type::Int,
                    span: empty_span(),
                },
                Param {
                    name: "b".into(),
                    ty: Type::Int,
                    span: empty_span(),
                },
            ],
            return_type: Type::Int,
            body: Block {
                statements: vec![Stmt::Return {
                    value: Some(bin(BinaryOp::Add, ident("a"), ident("b"))),
                    span: empty_span(),
                }],
                span: empty_span(),
            },
            span: empty_span(),
        }]
    );
}

#[test]
fn nested_blocks_and_let() {
    let src = "{ let x: int = 1; { let y = x; } }";
    let program = strip_program(parse(src).expect("parse ok"));
    assert_eq!(program.statements.len(), 1);
    match &program.statements[0] {
        Stmt::Block(outer) => {
            assert_eq!(outer.statements.len(), 2);
            assert!(matches!(
                &outer.statements[0],
                Stmt::Let {
                    name,
                    ty: Some(Type::Int),
                    ..
                } if name == "x"
            ));
            assert!(matches!(outer.statements[1], Stmt::Block(_)));
        }
        other => panic!("expected block, got {other:?}"),
    }
}

#[test]
fn missing_semicolon_reports_span() {
    let err = parse("let x = 1").expect_err("should fail");
    assert!(err.message.contains("`;`") || err.formatted.contains("`;`"));
    assert_eq!(err.span.start.line, 1);
    assert!(err.formatted.contains('^'));
}

#[test]
fn missing_paren_in_call() {
    let err = parse("print(1;").expect_err("should fail");
    assert!(err.formatted.contains('^'));
    assert!(err.expected.is_some() || err.message.contains(')'));
}

#[test]
fn invalid_assignment_target() {
    let err = parse("1 = 2;").expect_err("should fail");
    assert!(err.message.contains("assignment"));
    assert_eq!(err.span.start, Position::new(1, 1));
    assert!(err.formatted.contains('^'));
}

#[test]
fn fib_example_parses() {
    let src = include_str!("../../../examples/fib.tiny");
    let program = parse(src).expect("fib.tiny should parse");
    assert_eq!(program.functions.len(), 1);
    assert_eq!(program.functions[0].name, "fib");
    assert_eq!(program.functions[0].params.len(), 1);
    assert_eq!(program.functions[0].return_type, Type::Int);
    assert_eq!(program.statements.len(), 1);
    match &program.statements[0] {
        Stmt::Expr(Expr::Call { callee, args, .. }) => {
            assert_eq!(callee, "print");
            assert_eq!(args.len(), 1);
        }
        other => panic!("expected print call, got {other:?}"),
    }
}
