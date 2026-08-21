//! Lower AST to three-address IR.

use crate::ir::{CmpOp, Instr, IrFunction, Label, Module, PrintKind, Reg};
use std::collections::HashMap;
use tiny_ast::{BinaryOp, Block, Expr, FnDecl, Program, Stmt, Type, UnaryOp};

struct Lower {
    instrs: Vec<Instr>,
    strings: Vec<String>,
    next_reg: u32,
    next_label: u32,
    locals: Vec<HashMap<String, (Reg, Type)>>,
    /// function name -> return type
    fn_returns: HashMap<String, Type>,
}

impl Lower {
    fn new(fn_returns: HashMap<String, Type>) -> Self {
        Self {
            instrs: Vec::new(),
            strings: Vec::new(),
            next_reg: 0,
            next_label: 0,
            locals: vec![HashMap::new()],
            fn_returns,
        }
    }

    fn fresh_reg(&mut self) -> Reg {
        let r = Reg(self.next_reg);
        self.next_reg += 1;
        r
    }

    fn fresh_label(&mut self) -> Label {
        let l = Label(self.next_label);
        self.next_label += 1;
        l
    }

    fn push_scope(&mut self) {
        self.locals.push(HashMap::new());
    }

    fn pop_scope(&mut self) {
        if self.locals.len() > 1 {
            self.locals.pop();
        }
    }

    fn declare(&mut self, name: String, reg: Reg, ty: Type) {
        if let Some(scope) = self.locals.last_mut() {
            scope.insert(name, (reg, ty));
        }
    }

    fn lookup(&self, name: &str) -> Option<(Reg, Type)> {
        for scope in self.locals.iter().rev() {
            if let Some(v) = scope.get(name) {
                return Some(v.clone());
            }
        }
        None
    }

    fn emit(&mut self, instr: Instr) {
        self.instrs.push(instr);
    }

    fn intern_str(&mut self, s: &str) -> usize {
        if let Some(i) = self.strings.iter().position(|x| x == s) {
            return i;
        }
        self.strings.push(s.to_string());
        self.strings.len() - 1
    }

    fn finish(self, name: String, param_count: usize) -> IrFunction {
        IrFunction {
            name,
            param_count,
            instructions: self.instrs,
            string_pool: self.strings,
        }
    }

    fn lower_fn(func: &FnDecl, fn_returns: &HashMap<String, Type>) -> IrFunction {
        let mut low = Self::new(fn_returns.clone());
        for (i, p) in func.params.iter().enumerate() {
            let r = Reg(i as u32);
            low.next_reg = low.next_reg.max(i as u32 + 1);
            low.declare(p.name.clone(), r, p.ty.clone());
        }
        low.lower_block(&func.body, false);
        if !matches!(low.instrs.last(), Some(Instr::Return(_))) {
            low.emit(Instr::Return(None));
        }
        low.finish(func.name.clone(), func.params.len())
    }

    fn lower_main(stmts: &[Stmt], fn_returns: &HashMap<String, Type>) -> IrFunction {
        let mut low = Self::new(fn_returns.clone());
        for stmt in stmts {
            low.lower_stmt(stmt);
        }
        if !matches!(low.instrs.last(), Some(Instr::Return(_))) {
            low.emit(Instr::Return(None));
        }
        low.finish("__main".into(), 0)
    }

    fn lower_block(&mut self, block: &Block, new_scope: bool) {
        if new_scope {
            self.push_scope();
        }
        for stmt in &block.statements {
            self.lower_stmt(stmt);
        }
        if new_scope {
            self.pop_scope();
        }
    }

    fn lower_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let { name, ty, init, .. } => {
                let (r, inferred) = self.lower_expr(init);
                let t = ty.clone().unwrap_or(inferred);
                self.declare(name.clone(), r, t);
            }
            Stmt::Expr(e) => {
                let _ = self.lower_expr(e);
            }
            Stmt::Return { value, .. } => {
                let r = value.as_ref().map(|e| self.lower_expr(e).0);
                self.emit(Instr::Return(r));
            }
            Stmt::If {
                cond,
                then_block,
                else_block,
                ..
            } => {
                let (c, _) = self.lower_expr(cond);
                let else_l = self.fresh_label();
                let end_l = self.fresh_label();
                self.emit(Instr::JumpIfFalse(c, else_l));
                self.lower_block(then_block, true);
                self.emit(Instr::Jump(end_l));
                self.emit(Instr::Label(else_l));
                if let Some(eb) = else_block {
                    self.lower_block(eb, true);
                }
                self.emit(Instr::Label(end_l));
            }
            Stmt::While { cond, body, .. } => {
                let head = self.fresh_label();
                let end = self.fresh_label();
                self.emit(Instr::Label(head));
                let (c, _) = self.lower_expr(cond);
                self.emit(Instr::JumpIfFalse(c, end));
                self.lower_block(body, true);
                self.emit(Instr::Jump(head));
                self.emit(Instr::Label(end));
            }
            Stmt::Block(b) => self.lower_block(b, true),
        }
    }

    fn lower_expr(&mut self, expr: &Expr) -> (Reg, Type) {
        match expr {
            Expr::IntLit { value, .. } => {
                let d = self.fresh_reg();
                self.emit(Instr::ConstInt(d, *value));
                (d, Type::Int)
            }
            Expr::BoolLit { value, .. } => {
                let d = self.fresh_reg();
                self.emit(Instr::ConstBool(d, *value));
                (d, Type::Bool)
            }
            Expr::StringLit { value, .. } => {
                let idx = self.intern_str(value);
                let d = self.fresh_reg();
                self.emit(Instr::ConstStr(d, idx));
                (d, Type::String)
            }
            Expr::Ident { name, .. } => self
                .lookup(name)
                .unwrap_or_else(|| (self.fresh_reg(), Type::Int)),
            Expr::Unary { op, expr, .. } => {
                let (a, _) = self.lower_expr(expr);
                let d = self.fresh_reg();
                match op {
                    UnaryOp::Neg => {
                        self.emit(Instr::Neg(d, a));
                        (d, Type::Int)
                    }
                    UnaryOp::Not => {
                        self.emit(Instr::Not(d, a));
                        (d, Type::Bool)
                    }
                }
            }
            Expr::Binary {
                op, left, right, ..
            } => self.lower_binary(*op, left, right),
            Expr::Assign { name, value, .. } => {
                let (v, ty) = self.lower_expr(value);
                if let Some((dest, _)) = self.lookup(name) {
                    self.emit(Instr::Mov(dest, v));
                    (dest, ty)
                } else {
                    (v, ty)
                }
            }
            Expr::Call {
                callee, args, ..
            } => {
                let mut arg_regs = Vec::new();
                let mut first_ty = Type::Int;
                for (i, a) in args.iter().enumerate() {
                    let (r, t) = self.lower_expr(a);
                    if i == 0 {
                        first_ty = t;
                    }
                    arg_regs.push(r);
                }
                if callee == "print" {
                    let kind = match first_ty {
                        Type::Bool => PrintKind::Bool,
                        Type::String => PrintKind::String,
                        _ => PrintKind::Int,
                    };
                    if let Some(r) = arg_regs.first() {
                        self.emit(Instr::Print(*r, kind));
                    }
                    (self.fresh_reg(), Type::Void)
                } else {
                    let ret = self
                        .fn_returns
                        .get(callee)
                        .cloned()
                        .unwrap_or(Type::Int);
                    let d = self.fresh_reg();
                    self.emit(Instr::Call {
                        dest: if ret == Type::Void {
                            None
                        } else {
                            Some(d)
                        },
                        name: callee.clone(),
                        args: arg_regs,
                    });
                    (d, ret)
                }
            }
        }
    }

    fn lower_binary(&mut self, op: BinaryOp, left: &Expr, right: &Expr) -> (Reg, Type) {
        if matches!(op, BinaryOp::And) {
            let d = self.fresh_reg();
            let false_l = self.fresh_label();
            let end_l = self.fresh_label();
            let (l, _) = self.lower_expr(left);
            self.emit(Instr::JumpIfFalse(l, false_l));
            let (r, _) = self.lower_expr(right);
            self.emit(Instr::Mov(d, r));
            self.emit(Instr::Jump(end_l));
            self.emit(Instr::Label(false_l));
            self.emit(Instr::ConstBool(d, false));
            self.emit(Instr::Label(end_l));
            return (d, Type::Bool);
        }
        if matches!(op, BinaryOp::Or) {
            let d = self.fresh_reg();
            let false_l = self.fresh_label();
            let end_l = self.fresh_label();
            let (l, _) = self.lower_expr(left);
            self.emit(Instr::JumpIfFalse(l, false_l));
            self.emit(Instr::ConstBool(d, true));
            self.emit(Instr::Jump(end_l));
            self.emit(Instr::Label(false_l));
            let (r, _) = self.lower_expr(right);
            self.emit(Instr::Mov(d, r));
            self.emit(Instr::Label(end_l));
            return (d, Type::Bool);
        }

        let (a, _) = self.lower_expr(left);
        let (b, _) = self.lower_expr(right);
        let d = self.fresh_reg();
        let ty = match op {
            BinaryOp::Add => {
                self.emit(Instr::Add(d, a, b));
                Type::Int
            }
            BinaryOp::Sub => {
                self.emit(Instr::Sub(d, a, b));
                Type::Int
            }
            BinaryOp::Mul => {
                self.emit(Instr::Mul(d, a, b));
                Type::Int
            }
            BinaryOp::Div => {
                self.emit(Instr::Div(d, a, b));
                Type::Int
            }
            BinaryOp::Rem => {
                self.emit(Instr::Mod(d, a, b));
                Type::Int
            }
            BinaryOp::Eq => {
                self.emit(Instr::Cmp(CmpOp::Eq, d, a, b));
                Type::Bool
            }
            BinaryOp::Ne => {
                self.emit(Instr::Cmp(CmpOp::Ne, d, a, b));
                Type::Bool
            }
            BinaryOp::Lt => {
                self.emit(Instr::Cmp(CmpOp::Lt, d, a, b));
                Type::Bool
            }
            BinaryOp::Le => {
                self.emit(Instr::Cmp(CmpOp::Le, d, a, b));
                Type::Bool
            }
            BinaryOp::Gt => {
                self.emit(Instr::Cmp(CmpOp::Gt, d, a, b));
                Type::Bool
            }
            BinaryOp::Ge => {
                self.emit(Instr::Cmp(CmpOp::Ge, d, a, b));
                Type::Bool
            }
            BinaryOp::And | BinaryOp::Or => Type::Bool,
        };
        (d, ty)
    }
}

/// Lower a type-checked program to IR.
pub fn lower(program: &Program) -> Module {
    let fn_returns: HashMap<String, Type> = program
        .functions
        .iter()
        .map(|f| (f.name.clone(), f.return_type.clone()))
        .collect();
    let functions = program
        .functions
        .iter()
        .map(|f| Lower::lower_fn(f, &fn_returns))
        .collect();
    let main = Lower::lower_main(&program.statements, &fn_returns);
    Module { functions, main }
}
