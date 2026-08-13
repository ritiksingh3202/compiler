//! Two-pass type checker for Tiny.

use crate::error::{SemaError, SemaErrors};
use crate::symbols::{FnSig, ScopeStack};
use std::collections::HashMap;
use tiny_ast::{BinaryOp, Block, Expr, FnDecl, Program, Stmt, Type, UnaryOp};
use tiny_lexer::Span;

/// Poison type used only inside the checker to suppress cascading errors.
#[derive(Debug, Clone, PartialEq, Eq)]
enum Ty {
    Known(Type),
    Error,
}

impl Ty {
    fn as_known(&self) -> Option<&Type> {
        match self {
            Self::Known(t) => Some(t),
            Self::Error => None,
        }
    }
}

/// Type-check `program` against `source` (for diagnostics).
///
/// Pass 1 registers all function signatures. Pass 2 checks bodies and top-level
/// statements. Errors are collected for the whole program; analysis continues
/// after most failures using an internal poison type (not exposed as `Type`).
pub fn check(program: &Program, source: &str) -> Result<(), SemaErrors> {
    let mut checker = Checker::new(source);
    checker.collect_functions(program);
    checker.check_program(program);
    SemaErrors::from_vec(checker.errors)
}

struct Checker<'a> {
    source: &'a str,
    functions: HashMap<String, FnSig>,
    scopes: ScopeStack,
    /// Return type of the enclosing function (`Void` at top level).
    expected_return: Type,
    errors: Vec<SemaError>,
}

impl<'a> Checker<'a> {
    fn new(source: &'a str) -> Self {
        Self {
            source,
            functions: HashMap::new(),
            scopes: ScopeStack::new(),
            expected_return: Type::Void,
            errors: Vec::new(),
        }
    }

    fn error(&mut self, message: impl Into<String>, span: Span) {
        self.errors
            .push(SemaError::new(message, span, self.source));
    }

    fn collect_functions(&mut self, program: &Program) {
        for func in &program.functions {
            if self.functions.contains_key(&func.name) {
                self.error(
                    format!("function `{}` is already defined", func.name),
                    func.span,
                );
                continue;
            }
            // Duplicate param names within one signature.
            let mut seen = HashMap::new();
            for p in &func.params {
                if seen.insert(p.name.clone(), ()).is_some() {
                    self.error(
                        format!(
                            "duplicate parameter `{}` in function `{}`",
                            p.name, func.name
                        ),
                        p.span,
                    );
                }
                if p.ty == Type::Void {
                    self.error("parameters cannot have type `void`", p.span);
                }
            }
            self.functions.insert(
                func.name.clone(),
                FnSig::from_params(
                    func.name.clone(),
                    &func.params,
                    func.return_type.clone(),
                    func.span,
                ),
            );
        }
    }

    fn check_program(&mut self, program: &Program) {
        for func in &program.functions {
            self.check_function(func);
        }

        // Top-level = implicit Void "main"
        self.expected_return = Type::Void;
        self.scopes = ScopeStack::new();
        for stmt in &program.statements {
            self.check_stmt(stmt);
        }
    }

    fn check_function(&mut self, func: &FnDecl) {
        self.expected_return = func.return_type.clone();
        self.scopes = ScopeStack::new();
        self.scopes.push();
        for p in &func.params {
            if self
                .scopes
                .declare(p.name.clone(), p.ty.clone())
                .is_some()
            {
                // Already reported in pass 1 for duplicates.
            }
        }
        self.check_block(&func.body, false);

        if func.return_type != Type::Void && !block_always_returns(&func.body) {
            self.error(
                format!(
                    "function `{}` must return a value of type `{}` on all paths",
                    func.name, func.return_type
                ),
                func.span,
            );
        }
        self.scopes.pop();
    }

    /// If `new_scope` is true, push/pop a scope around the block.
    fn check_block(&mut self, block: &Block, new_scope: bool) {
        if new_scope {
            self.scopes.push();
        }
        for stmt in &block.statements {
            self.check_stmt(stmt);
        }
        if new_scope {
            self.scopes.pop();
        }
    }

    fn check_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let {
                name,
                ty,
                init,
                span,
            } => {
                let init_ty = self.check_expr(init);
                let bound = match (ty, init_ty.as_known()) {
                    (Some(ann), Some(_)) if ann == &Type::Void => {
                        self.error("cannot declare a variable of type `void`", *span);
                        Ty::Error
                    }
                    (Some(ann), Some(got)) if ann != got => {
                        self.error(
                            format!(
                                "type mismatch in `let`: expected `{ann}`, found `{got}`"
                            ),
                            init.span(),
                        );
                        Ty::Known(ann.clone())
                    }
                    (Some(ann), _) => Ty::Known(ann.clone()),
                    (None, Some(Type::Void)) => {
                        self.error(
                            "cannot infer type of `void` expression in `let`",
                            init.span(),
                        );
                        Ty::Error
                    }
                    (None, Some(got)) => Ty::Known(got.clone()),
                    (None, None) => Ty::Error,
                };

                if let Ty::Known(t) = bound {
                    if self.scopes.declare(name.clone(), t).is_some() {
                        self.error(
                            format!("variable `{name}` is already declared in this scope"),
                            *span,
                        );
                    }
                }
            }
            Stmt::Expr(expr) => {
                let _ = self.check_expr(expr);
            }
            Stmt::Return { value, span } => {
                let expected = self.expected_return.clone();
                match (value, &expected) {
                    (None, Type::Void) => {}
                    (None, ret) => {
                        self.error(
                            format!("missing return value: expected `{ret}`"),
                            *span,
                        );
                    }
                    (Some(expr), Type::Void) => {
                        let _ = self.check_expr(expr);
                        self.error(
                            "cannot return a value from a `void` function",
                            expr.span(),
                        );
                    }
                    (Some(expr), expected) => {
                        let got = self.check_expr(expr);
                        if let Some(g) = got.as_known() {
                            if g != expected {
                                self.error(
                                    format!(
                                        "return type mismatch: expected `{expected}`, found `{g}`"
                                    ),
                                    expr.span(),
                                );
                            }
                        }
                    }
                }
            }
            Stmt::If {
                cond,
                then_block,
                else_block,
                ..
            } => {
                self.expect_bool(cond, "if condition");
                self.check_block(then_block, true);
                if let Some(else_b) = else_block {
                    self.check_block(else_b, true);
                }
            }
            Stmt::While { cond, body, .. } => {
                self.expect_bool(cond, "while condition");
                self.check_block(body, true);
            }
            Stmt::Block(block) => {
                self.check_block(block, true);
            }
        }
    }

    fn expect_bool(&mut self, expr: &Expr, what: &str) {
        let ty = self.check_expr(expr);
        if let Some(t) = ty.as_known() {
            if t != &Type::Bool {
                self.error(
                    format!("{what} must be `bool`, found `{t}`"),
                    expr.span(),
                );
            }
        }
    }

    fn check_expr(&mut self, expr: &Expr) -> Ty {
        match expr {
            Expr::IntLit { .. } => Ty::Known(Type::Int),
            Expr::StringLit { .. } => Ty::Known(Type::String),
            Expr::BoolLit { .. } => Ty::Known(Type::Bool),
            Expr::Ident { name, span } => match self.scopes.lookup(name) {
                Some(ty) => Ty::Known(ty.clone()),
                None => {
                    self.error(format!("undefined variable `{name}`"), *span);
                    Ty::Error
                }
            },
            Expr::Unary { op, expr: inner, span } => {
                let inner_ty = self.check_expr(inner);
                match (op, inner_ty.as_known()) {
                    (_, None) => Ty::Error,
                    (UnaryOp::Neg, Some(Type::Int)) => Ty::Known(Type::Int),
                    (UnaryOp::Neg, Some(t)) => {
                        self.error(
                            format!("unary `-` requires `int`, found `{t}`"),
                            *span,
                        );
                        Ty::Error
                    }
                    (UnaryOp::Not, Some(Type::Bool)) => Ty::Known(Type::Bool),
                    (UnaryOp::Not, Some(t)) => {
                        self.error(
                            format!("unary `!` requires `bool`, found `{t}`"),
                            *span,
                        );
                        Ty::Error
                    }
                }
            }
            Expr::Binary {
                op,
                left,
                right,
                span,
            } => self.check_binary(*op, left, right, *span),
            Expr::Assign { name, value, span } => {
                let value_ty = self.check_expr(value);
                match self.scopes.lookup(name).cloned() {
                    None => {
                        self.error(format!("undefined variable `{name}`"), *span);
                        Ty::Error
                    }
                    Some(expected) => {
                        if let Some(got) = value_ty.as_known() {
                            if got != &expected {
                                self.error(
                                    format!(
                                        "cannot assign `{got}` to `{name}` of type `{expected}`"
                                    ),
                                    value.span(),
                                );
                            }
                        }
                        Ty::Known(expected)
                    }
                }
            }
            Expr::Call {
                callee,
                args,
                span,
            } => self.check_call(callee, args, *span),
        }
    }

    fn check_binary(&mut self, op: BinaryOp, left: &Expr, right: &Expr, span: Span) -> Ty {
        // Short-circuit ops: still type-check both sides.
        let lt = self.check_expr(left);
        let rt = self.check_expr(right);
        let (Some(l), Some(r)) = (lt.as_known(), rt.as_known()) else {
            return Ty::Error;
        };

        match op {
            BinaryOp::Add
            | BinaryOp::Sub
            | BinaryOp::Mul
            | BinaryOp::Div
            | BinaryOp::Rem => {
                if l == &Type::Int && r == &Type::Int {
                    Ty::Known(Type::Int)
                } else {
                    self.error(
                        format!(
                            "arithmetic operator requires `int` operands, found `{l}` and `{r}`"
                        ),
                        span,
                    );
                    Ty::Error
                }
            }
            BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge => {
                if l == &Type::Int && r == &Type::Int {
                    Ty::Known(Type::Bool)
                } else {
                    self.error(
                        format!(
                            "comparison requires `int` operands, found `{l}` and `{r}`"
                        ),
                        span,
                    );
                    Ty::Error
                }
            }
            BinaryOp::Eq | BinaryOp::Ne => {
                if l == r && matches!(l, Type::Int | Type::Bool | Type::String) {
                    Ty::Known(Type::Bool)
                } else {
                    self.error(
                        format!("cannot compare `{l}` and `{r}` with `==`/`!=`"),
                        span,
                    );
                    Ty::Error
                }
            }
            BinaryOp::And | BinaryOp::Or => {
                if l == &Type::Bool && r == &Type::Bool {
                    Ty::Known(Type::Bool)
                } else {
                    self.error(
                        format!("logical operator requires `bool` operands, found `{l}` and `{r}`"),
                        span,
                    );
                    Ty::Error
                }
            }
        }
    }

    fn check_call(&mut self, callee: &str, args: &[Expr], span: Span) -> Ty {
        if callee == "print" {
            if args.len() != 1 {
                self.error(
                    format!("`print` takes 1 argument, found {}", args.len()),
                    span,
                );
                for a in args {
                    let _ = self.check_expr(a);
                }
                return Ty::Error;
            }
            let arg_ty = self.check_expr(&args[0]);
            match arg_ty.as_known() {
                Some(Type::Int | Type::Bool | Type::String) => Ty::Known(Type::Void),
                Some(t) => {
                    self.error(
                        format!("`print` argument must be `int`, `bool`, or `string`, found `{t}`"),
                        args[0].span(),
                    );
                    Ty::Error
                }
                None => Ty::Error,
            }
        } else {
            let Some(sig) = self.functions.get(callee).cloned() else {
                self.error(format!("undefined function `{callee}`"), span);
                for a in args {
                    let _ = self.check_expr(a);
                }
                return Ty::Error;
            };

            if args.len() != sig.params.len() {
                self.error(
                    format!(
                        "function `{callee}` takes {} argument(s), found {}",
                        sig.params.len(),
                        args.len()
                    ),
                    span,
                );
            }

            for (i, arg) in args.iter().enumerate() {
                let got = self.check_expr(arg);
                if let Some((_, expected)) = sig.params.get(i) {
                    if let Some(g) = got.as_known() {
                        if g != expected {
                            self.error(
                                format!(
                                    "argument {} to `{callee}`: expected `{expected}`, found `{g}`",
                                    i + 1
                                ),
                                arg.span(),
                            );
                        }
                    }
                }
            }

            Ty::Known(sig.return_type)
        }
    }
}

/// Textual end reachability: both branches of if/else must return; while does not count.
fn block_always_returns(block: &Block) -> bool {
    block
        .statements
        .iter()
        .any(stmt_always_returns)
}

fn stmt_always_returns(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Return { .. } => true,
        Stmt::If {
            then_block,
            else_block,
            ..
        } => match else_block {
            Some(else_b) => block_always_returns(then_block) && block_always_returns(else_b),
            None => false,
        },
        Stmt::Block(b) => block_always_returns(b),
        Stmt::Let { .. } | Stmt::Expr(_) | Stmt::While { .. } => false,
    }
}
