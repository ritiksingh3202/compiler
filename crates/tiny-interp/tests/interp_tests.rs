use tiny_interp::run_to_string;
use tiny_parser::parse;
use tiny_sema::check;

fn interpret(src: &str) -> String {
    let program = parse(src).expect("parse");
    check(&program, src).expect("sema");
    run_to_string(&program, src).expect("run")
}

#[test]
fn fib_output() {
    let src = include_str!("../../../examples/fib.tiny");
    assert_eq!(interpret(src).trim(), "55");
}

#[test]
fn loops_output() {
    let src = include_str!("../../../examples/loops.tiny");
    assert_eq!(interpret(src).trim(), "10");
}

#[test]
fn hello_output() {
    let src = include_str!("../../../examples/hello.tiny");
    assert_eq!(interpret(src).trim(), "true\nhello\n42");
}

#[test]
fn short_circuit_and() {
    let src = r#"
        fn boom() -> int { return 1 / 0; }
        print(false && boom() > 0);
    "#;
    assert_eq!(interpret(src).trim(), "false");
}

#[test]
fn rec_while_example() {
    let src = include_str!("../../../examples/rec_while.tiny");
    assert_eq!(interpret(src).trim(), "15");
}

#[test]
fn nested_ret_example() {
    let src = include_str!("../../../examples/nested_ret.tiny");
    assert_eq!(interpret(src).replace("\r\n", "\n").trim(), "-1\n0\n1\n2");
}

#[test]
fn mixed_types_example() {
    let src = include_str!("../../../examples/mixed_types.tiny");
    assert_eq!(
        interpret(src).replace("\r\n", "\n").trim(),
        "tiny\n85\ntrue\nB\nC"
    );
}
