//! Emit Linux x86-64 AT&T assembly from Tiny IR.

use tiny_ir::{CmpOp, Instr, IrFunction, Module, PrintKind, Reg};

/// Generate a complete `.s` file (AT&T syntax, System V AMD64).
pub fn emit_asm(module: &Module) -> String {
    let mut out = String::new();
    out.push_str(".text\n");
    out.push_str(".globl main\n");

    // Shared format strings
    out.push_str(".section .rodata\n");
    out.push_str(".fmt_int:\n");
    out.push_str("  .string \"%ld\\n\"\n");
    out.push_str(".fmt_str:\n");
    out.push_str("  .string \"%s\\n\"\n");
    out.push_str(".true_str:\n");
    out.push_str("  .string \"true\\n\"\n");
    out.push_str(".false_str:\n");
    out.push_str("  .string \"false\\n\"\n");

    // Per-function string pools
    for (fi, func) in module
        .functions
        .iter()
        .chain(std::iter::once(&module.main))
        .enumerate()
    {
        for (si, s) in func.string_pool.iter().enumerate() {
            out.push_str(&format!(".str_{fi}_{si}:\n"));
            out.push_str(&format!("  .string {}\n", escape_c_string(s)));
        }
    }

    out.push_str(".text\n");

    for (fi, func) in module.functions.iter().enumerate() {
        emit_function(&mut out, func, fi, false);
    }
    let main_idx = module.functions.len();
    emit_function(&mut out, &module.main, main_idx, true);

    out
}

fn escape_c_string(s: &str) -> String {
    let mut out = String::from("\"");
    for c in s.chars() {
        match c {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\t' => out.push_str("\\t"),
            '\r' => out.push_str("\\r"),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

fn slot(reg: Reg) -> i32 {
    -8 * (reg.0 as i32 + 1)
}

fn emit_function(out: &mut String, func: &IrFunction, fi: usize, is_main: bool) {
    let name = if is_main {
        "main"
    } else {
        // Prefix user functions to avoid clashing with libc
        out.push_str(&format!(".globl tiny_{}\n", func.name));
        // We'll emit label tiny_name
        ""
    };

    if is_main {
        out.push_str("main:\n");
    } else {
        out.push_str(&format!("tiny_{}:\n", func.name));
        let _ = name;
    }

    // Prologue
    let max_reg = func
        .instructions
        .iter()
        .filter_map(instr_max_reg)
        .max()
        .unwrap_or(0);
    let frame = ((max_reg as i32 + 1) * 8 + 15) & !15; // 16-byte align

    out.push_str("  pushq %rbp\n");
    out.push_str("  movq %rsp, %rbp\n");
    if frame > 0 {
        out.push_str(&format!("  subq ${frame}, %rsp\n"));
    }

    // Move params from arg regs into slots
    let arg_regs = ["%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"];
    for i in 0..func.param_count.min(6) {
        let s = slot(Reg(i as u32));
        out.push_str(&format!("  movq {}, {}(%rbp)\n", arg_regs[i], s));
    }

    for instr in &func.instructions {
        emit_instr(out, instr, fi, &func.name, is_main);
    }

    // Epilogue fallback
    out.push_str(&format!(".Lret_{fi}:\n"));
    out.push_str("  leave\n");
    out.push_str("  ret\n");
}

fn instr_max_reg(instr: &Instr) -> Option<u32> {
    match instr {
        Instr::ConstInt(d, _)
        | Instr::ConstBool(d, _)
        | Instr::ConstStr(d, _)
        | Instr::Neg(d, _)
        | Instr::Not(d, _) => Some(d.0),
        Instr::Mov(d, s) => Some(d.0.max(s.0)),
        Instr::Add(d, a, b)
        | Instr::Sub(d, a, b)
        | Instr::Mul(d, a, b)
        | Instr::Div(d, a, b)
        | Instr::Mod(d, a, b)
        | Instr::Cmp(_, d, a, b) => Some(d.0.max(a.0).max(b.0)),
        Instr::JumpIfFalse(r, _) | Instr::Print(r, _) => Some(r.0),
        Instr::Call { dest, args, .. } => {
            let mut m = dest.map(|d| d.0).unwrap_or(0);
            for a in args {
                m = m.max(a.0);
            }
            Some(m)
        }
        Instr::Return(Some(r)) => Some(r.0),
        _ => None,
    }
}

fn label_name(func_name: &str, is_main: bool, l: u32) -> String {
    if is_main {
        format!(".Lm_{l}")
    } else {
        format!(".L_{func_name}_{l}")
    }
}

fn emit_instr(out: &mut String, instr: &Instr, fi: usize, func_name: &str, is_main: bool) {
    match instr {
        Instr::ConstInt(d, v) => {
            out.push_str(&format!("  movq ${v}, {}(%rbp)\n", slot(*d)));
        }
        Instr::ConstBool(d, v) => {
            let n = if *v { 1 } else { 0 };
            out.push_str(&format!("  movq ${n}, {}(%rbp)\n", slot(*d)));
        }
        Instr::ConstStr(d, idx) => {
            out.push_str(&format!(
                "  leaq .str_{fi}_{idx}(%rip), %rax\n"
            ));
            out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
        }
        Instr::Mov(d, s) => {
            out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*s)));
            out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
        }
        Instr::Add(d, a, b) => bin_op(out, d, a, b, "addq"),
        Instr::Sub(d, a, b) => bin_op(out, d, a, b, "subq"),
        Instr::Mul(d, a, b) => {
            out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*a)));
            out.push_str(&format!("  imulq {}(%rbp), %rax\n", slot(*b)));
            out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
        }
        Instr::Div(d, a, b) | Instr::Mod(d, a, b) => {
            out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*a)));
            out.push_str("  cqo\n");
            out.push_str(&format!("  idivq {}(%rbp)\n", slot(*b)));
            if matches!(instr, Instr::Mod(_, _, _)) {
                out.push_str(&format!("  movq %rdx, {}(%rbp)\n", slot(*d)));
            } else {
                out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
            }
        }
        Instr::Neg(d, a) => {
            out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*a)));
            out.push_str("  negq %rax\n");
            out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
        }
        Instr::Not(d, a) => {
            out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*a)));
            out.push_str("  xorq $1, %rax\n");
            out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
        }
        Instr::Cmp(op, d, a, b) => {
            out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*a)));
            out.push_str(&format!("  cmpq {}(%rbp), %rax\n", slot(*b)));
            let set = match op {
                CmpOp::Eq => "sete",
                CmpOp::Ne => "setne",
                CmpOp::Lt => "setl",
                CmpOp::Le => "setle",
                CmpOp::Gt => "setg",
                CmpOp::Ge => "setge",
            };
            out.push_str(&format!("  {set} %al\n"));
            out.push_str("  movzbq %al, %rax\n");
            out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
        }
        Instr::Label(l) => {
            out.push_str(&format!("{}:\n", label_name(func_name, is_main, l.0)));
        }
        Instr::Jump(l) => {
            out.push_str(&format!(
                "  jmp {}\n",
                label_name(func_name, is_main, l.0)
            ));
        }
        Instr::JumpIfFalse(r, l) => {
            out.push_str(&format!("  cmpq $0, {}(%rbp)\n", slot(*r)));
            out.push_str(&format!(
                "  je {}\n",
                label_name(func_name, is_main, l.0)
            ));
        }
        Instr::Call { dest, name, args } => {
            let arg_regs = ["%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"];
            for (i, a) in args.iter().enumerate().take(6) {
                out.push_str(&format!(
                    "  movq {}(%rbp), {}\n",
                    slot(*a),
                    arg_regs[i]
                ));
            }
            // 16-byte stack alignment before call: frame already aligned
            out.push_str("  xorl %eax, %eax\n");
            out.push_str(&format!("  call tiny_{name}\n"));
            if let Some(d) = dest {
                out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
            }
        }
        Instr::Return(v) => {
            if let Some(r) = v {
                out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*r)));
            } else {
                out.push_str("  xorq %rax, %rax\n");
            }
            out.push_str(&format!("  jmp .Lret_{fi}\n"));
        }
        Instr::Print(r, kind) => match kind {
            PrintKind::Int => {
                out.push_str(&format!("  movq {}(%rbp), %rsi\n", slot(*r)));
                out.push_str("  leaq .fmt_int(%rip), %rdi\n");
                out.push_str("  xorl %eax, %eax\n");
                out.push_str("  call printf\n");
            }
            PrintKind::String => {
                out.push_str(&format!("  movq {}(%rbp), %rsi\n", slot(*r)));
                out.push_str("  leaq .fmt_str(%rip), %rdi\n");
                out.push_str("  xorl %eax, %eax\n");
                out.push_str("  call printf\n");
            }
            PrintKind::Bool => {
                out.push_str(&format!("  cmpq $0, {}(%rbp)\n", slot(*r)));
                out.push_str("  leaq .false_str(%rip), %rdi\n");
                out.push_str("  leaq .true_str(%rip), %rax\n");
                out.push_str("  cmovne %rax, %rdi\n");
                out.push_str("  xorl %eax, %eax\n");
                out.push_str("  call printf\n");
            }
        },
    }
}

fn bin_op(out: &mut String, d: &Reg, a: &Reg, b: &Reg, op: &str) {
    out.push_str(&format!("  movq {}(%rbp), %rax\n", slot(*a)));
    out.push_str(&format!("  {op} {}(%rbp), %rax\n", slot(*b)));
    out.push_str(&format!("  movq %rax, {}(%rbp)\n", slot(*d)));
}
