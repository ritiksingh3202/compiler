use tiny_ast::Type;
use tiny_lexer::Position;
use tiny_parser::parse;
use tiny_sema::{check, SemaErrors};

fn ok(src: &str) {
    let program = parse(src).expect("parse");
    check(&program, src).expect("sema should pass");
}

fn err(src: &str) -> SemaErrors {
    let program = parse(src).expect("parse");
    check(&program, src).expect_err("sema should fail")
}

fn first_msg(src: &str) -> (String, Position) {
    let e = err(src);
    let first = &e.errors[0];
    (first.message.clone(), first.span.start)
}

#[test]
fn fib_passes() {
    let src = include_str!("../../../examples/fib.tiny");
    ok(src);
}

#[test]
fn shadowing_in_nested_scope() {
    ok(r#"
        fn main() {
            let x: int = 1;
            {
                let x: int = 2;
                print(x);
            }
            print(x);
        }
    "#);
}

#[test]
fn use_before_declare() {
    let (msg, pos) = first_msg("print(x); let x = 1;");
    assert!(msg.contains("undefined variable `x`"));
    assert_eq!(pos, Position::new(1, 7));
}

#[test]
fn arithmetic_and_comparison_types() {
    ok("let a = 1 + 2 * 3; let b = a < 10; print(b);");
}

#[test]
fn logical_and_unary() {
    ok("let a = true && !false; let b = -1; print(a); print(b);");
}

#[test]
fn equality_on_string_and_bool() {
    ok(r#"let s = "hi" == "hi"; let t = true != false; print(s);"#);
}

#[test]
fn let_with_and_without_annotation() {
    ok("let x: int = 1; let y = x; print(y);");
}

#[test]
fn let_annotation_mismatch() {
    let (msg, pos) = first_msg("let x: bool = 1;");
    assert!(msg.contains("type mismatch"));
    assert_eq!(pos.line, 1);
}

#[test]
fn call_arg_mismatch() {
    let (msg, _) = first_msg(
        r#"
        fn f(n: int) -> int { return n; }
        f(true);
    "#,
    );
    assert!(msg.contains("expected `int`") || msg.contains("argument"));
}

#[test]
fn mutual_recursion_and_forward_ref() {
    ok(r#"
        fn a(n: int) -> int {
            if n < 1 { return 0; }
            return b(n - 1);
        }
        fn b(n: int) -> int {
            return a(n);
        }
        print(a(3));
    "#);
}

#[test]
fn missing_return_path() {
    let (msg, _) = first_msg(
        r#"
        fn f(n: int) -> int {
            if n < 0 { return 0; }
        }
    "#,
    );
    assert!(msg.contains("must return"));
}

#[test]
fn both_branches_return_ok() {
    ok(r#"
        fn f(n: int) -> int {
            if n < 0 { return 0; } else { return 1; }
        }
        print(f(1));
    "#);
}

#[test]
fn void_function_and_return() {
    ok(r#"
        fn log(n: int) {
            print(n);
            return;
        }
        log(1);
    "#);
}

#[test]
fn return_value_in_void_fn() {
    let (msg, _) = first_msg(
        r#"
        fn f() {
            return 1;
        }
    "#,
    );
    assert!(msg.contains("void"));
}

#[test]
fn print_each_type() {
    ok(r#"print(1); print(true); print("hi");"#);
}

#[test]
fn print_wrong_arity() {
    let (msg, pos) = first_msg("print(1, 2);");
    assert!(msg.contains("print"));
    assert_eq!(pos, Position::new(1, 1));
}

#[test]
fn undefined_function() {
    let (msg, pos) = first_msg("foo(1);");
    assert!(msg.contains("undefined function `foo`"));
    assert_eq!(pos, Position::new(1, 1));
}

#[test]
fn assign_type_mismatch() {
    let (msg, _) = first_msg("let x: int = 1; x = true;");
    assert!(msg.contains("cannot assign"));
}

#[test]
fn if_condition_not_bool() {
    let (msg, _) = first_msg("if 1 { print(1); }");
    assert!(msg.contains("bool"));
}

#[test]
fn error_has_caret() {
    let e = err("let x: int = true;");
    assert!(e.errors[0].formatted.contains('^'));
    assert!(e.errors[0].formatted.contains("-->"));
}

#[test]
fn void_return_type_is_void() {
    let src = "fn f() { return; }";
    let program = parse(src).expect("parse");
    assert_eq!(program.functions[0].return_type, Type::Void);
    ok(src);
}
