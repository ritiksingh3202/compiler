//! Three-address-code IR.

use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Reg(pub u32);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Label(pub u32);

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CmpOp {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PrintKind {
    Int,
    Bool,
    String,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Instr {
    /// dest = imm
    ConstInt(Reg, i64),
    ConstBool(Reg, bool),
    /// dest = pointer to string in pool (index)
    ConstStr(Reg, usize),
    Mov(Reg, Reg),
    Add(Reg, Reg, Reg),
    Sub(Reg, Reg, Reg),
    Mul(Reg, Reg, Reg),
    Div(Reg, Reg, Reg),
    Mod(Reg, Reg, Reg),
    Neg(Reg, Reg),
    Not(Reg, Reg),
    Cmp(CmpOp, Reg, Reg, Reg),
    Jump(Label),
    JumpIfFalse(Reg, Label),
    Label(Label),
    Call {
        dest: Option<Reg>,
        name: String,
        args: Vec<Reg>,
    },
    Return(Option<Reg>),
    Print(Reg, PrintKind),
}

#[derive(Debug, Clone, PartialEq)]
pub struct IrFunction {
    pub name: String,
    pub param_count: usize,
    pub instructions: Vec<Instr>,
    pub string_pool: Vec<String>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Module {
    pub functions: Vec<IrFunction>,
    /// Implicit main from top-level statements.
    pub main: IrFunction,
}

impl fmt::Display for Reg {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "r{}", self.0)
    }
}

impl fmt::Display for Label {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "L{}", self.0)
    }
}

impl fmt::Display for Instr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Instr::ConstInt(d, v) => write!(f, "{d} = {v}"),
            Instr::ConstBool(d, v) => write!(f, "{d} = {v}"),
            Instr::ConstStr(d, i) => write!(f, "{d} = str[{i}]"),
            Instr::Mov(d, s) => write!(f, "{d} = {s}"),
            Instr::Add(d, a, b) => write!(f, "{d} = {a} + {b}"),
            Instr::Sub(d, a, b) => write!(f, "{d} = {a} - {b}"),
            Instr::Mul(d, a, b) => write!(f, "{d} = {a} * {b}"),
            Instr::Div(d, a, b) => write!(f, "{d} = {a} / {b}"),
            Instr::Mod(d, a, b) => write!(f, "{d} = {a} % {b}"),
            Instr::Neg(d, a) => write!(f, "{d} = -{a}"),
            Instr::Not(d, a) => write!(f, "{d} = !{a}"),
            Instr::Cmp(op, d, a, b) => {
                let sym = match op {
                    CmpOp::Eq => "==",
                    CmpOp::Ne => "!=",
                    CmpOp::Lt => "<",
                    CmpOp::Le => "<=",
                    CmpOp::Gt => ">",
                    CmpOp::Ge => ">=",
                };
                write!(f, "{d} = {a} {sym} {b}")
            }
            Instr::Jump(l) => write!(f, "goto {l}"),
            Instr::JumpIfFalse(r, l) => write!(f, "if !{r} goto {l}"),
            Instr::Label(l) => write!(f, "{l}:"),
            Instr::Call { dest, name, args } => {
                let args_s: Vec<_> = args.iter().map(|r| r.to_string()).collect();
                match dest {
                    Some(d) => write!(f, "{d} = call {name}({})", args_s.join(", ")),
                    None => write!(f, "call {name}({})", args_s.join(", ")),
                }
            }
            Instr::Return(None) => write!(f, "return"),
            Instr::Return(Some(r)) => write!(f, "return {r}"),
            Instr::Print(r, PrintKind::Int) => write!(f, "print_int {r}"),
            Instr::Print(r, PrintKind::Bool) => write!(f, "print_bool {r}"),
            Instr::Print(r, PrintKind::String) => write!(f, "print_str {r}"),
        }
    }
}

impl IrFunction {
    pub fn display(&self) -> String {
        let mut out = format!("fn {} (params={}):\n", self.name, self.param_count);
        for (i, s) in self.string_pool.iter().enumerate() {
            out.push_str(&format!("  str[{i}] = {s:?}\n"));
        }
        for instr in &self.instructions {
            if matches!(instr, Instr::Label(_)) {
                out.push_str(&format!("{instr}\n"));
            } else {
                out.push_str(&format!("  {instr}\n"));
            }
        }
        out
    }
}

impl Module {
    pub fn display(&self) -> String {
        let mut out = String::new();
        for f in &self.functions {
            out.push_str(&f.display());
            out.push('\n');
        }
        out.push_str(&self.main.display());
        out
    }
}
