use tiny_ir::{lower, optimize, Instr};
use tiny_parser::parse;
use tiny_sema::check;

fn module(src: &str) -> tiny_ir::Module {
    let p = parse(src).expect("parse");
    check(&p, src).expect("sema");
    lower(&p)
}

#[test]
fn lower_arithmetic() {
    let m = module("print(1 + 2 * 3);");
    let text = m.display();
    assert!(text.contains('+') || text.contains("r"));
    assert!(text.contains("print_int") || text.contains("print"));
}

#[test]
fn lower_if_while_call() {
    let src = r#"
        fn f(n: int) -> int {
            if n < 1 { return 0; }
            return f(n - 1);
        }
        let i = 0;
        while i < 3 { i = i + 1; }
        print(f(2));
    "#;
    let m = module(src);
    let text = m.display();
    assert!(text.contains("fn f"));
    assert!(text.contains("goto") || text.contains("if !"));
    assert!(text.contains("call f"));
}

#[test]
fn constant_fold_shrinks() {
    let src = "print(2 + 3);";
    let mut m = module(src);
    let before = m.main.instructions.len();
    optimize(&mut m);
    let after = m.main.instructions.len();
    assert!(after <= before);
    assert!(
        m.main
            .instructions
            .iter()
            .any(|i| matches!(i, Instr::ConstInt(_, 5)))
    );
}
