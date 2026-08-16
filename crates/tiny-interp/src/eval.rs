//! Tree-walking evaluator.

use crate::error::RuntimeError;
use crate::value::Value;
use std::collections::HashMap;
use std::io::Write;
use tiny_ast::{BinaryOp, Block, Expr, FnDecl, Program, Stmt, UnaryOp};
use tiny_lexer::Span;

type Env = Vec<HashMap<String, Value>>;

/// Evaluate a type-checked program, writing `print` output to `out`.
pub fn run(program: &Program, source: &str, out: &mut dyn Write) -> Result<(), RuntimeError> {
    let mut interp = Interpreter::new(program, source, out);
    interp.run_program()
}

/// Convenience: run and capture stdout as a String.
pub fn run_to_string(program: &Program, source: &str) -> Result<String, RuntimeError> {
    let mut buf = Vec::new();
    run(program, source, &mut buf)?;
    String::from_utf8(buf).map_err(|e| {
        RuntimeError::new(
            format!("output is not valid UTF-8: {e}"),
            program.span,
            source,
        )
    })
}

struct Interpreter<'a> {
    program: &'a Program,
    source: &'a str,
    out: &'a mut dyn Write,
    functions: HashMap<&'a str, &'a FnDecl>,
    env: Env,
}

enum Flow {
    Next,
    Return(Value),
}

impl<'a> Interpreter<'a> {
    fn new(program: &'a Program, source: &'a str, out: &'a mut dyn Write) -> Self {
        let mut functions = HashMap::new();
        for f in &program.functions {
            functions.insert(f.name.as_str(), f);
        }
        Self {
            program,
            source,
            out,
            functions,
            env: vec![HashMap::new()],
        }
    }

    fn err(&self, message: impl Into<String>, span: Span) -> RuntimeError {
        RuntimeError::new(message, span, self.source)
    }

    fn run_program(&mut self) -> Result<(), RuntimeError> {
        for stmt in &self.program.statements {
            match self.exec_stmt(stmt)? {
                Flow::Next => {}
                Flow::Return(_) => break,
            }
        }
        Ok(())
    }

    fn push_scope(&mut self) {
        self.env.push(HashMap::new());
    }

    fn pop_scope(&mut self) {
        if self.env.len() > 1 {
            self.env.pop();
        }
    }

    fn declare(&mut self, name: String, value: Value) {
        if let Some(scope) = self.env.last_mut() {
            scope.insert(name, value);
        }
    }

    fn assign(&mut self, name: &str, value: Value, span: Span) -> Result<(), RuntimeError> {
        for scope in self.env.iter_mut().rev() {
            if scope.contains_key(name) {
                scope.insert(name.to_string(), value);
                return Ok(());
            }
        }
        Err(self.err(format!("undefined variable `{name}`"), span))
    }

    fn lookup(&self, name: &str, span: Span) -> Result<Value, RuntimeError> {
        for scope in self.env.iter().rev() {
            if let Some(v) = scope.get(name) {
                return Ok(v.clone());
            }
        }
        Err(self.err(format!("undefined variable `{name}`"), span))
    }

    fn exec_block(&mut self, block: &Block) -> Result<Flow, RuntimeError> {
        self.push_scope();
        let mut flow = Flow::Next;
        for stmt in &block.statements {
            flow = self.exec_stmt(stmt)?;
            if matches!(flow, Flow::Return(_)) {
                break;
            }
        }
        self.pop_scope();
        Ok(flow)
    }

    fn exec_stmt(&mut self, stmt: &Stmt) -> Result<Flow, RuntimeError> {
        match stmt {
            Stmt::Let { name, init, .. } => {
                let v = self.eval(init)?;
                self.declare(name.clone(), v);
                Ok(Flow::Next)
            }
            Stmt::Expr(e) => {
                let _ = self.eval(e)?;
                Ok(Flow::Next)
            }
            Stmt::Return { value, .. } => {
                let v = match value {
                    Some(e) => self.eval(e)?,
                    None => Value::Void,
                };
                Ok(Flow::Return(v))
            }
            Stmt::If {
                cond,
                then_block,
                else_block,
                ..
            } => {
                if self.eval(cond)?.as_bool() {
                    self.exec_block(then_block)
                } else if let Some(eb) = else_block {
                    self.exec_block(eb)
                } else {
                    Ok(Flow::Next)
                }
            }
            Stmt::While { cond, body, .. } => {
                while self.eval(cond)?.as_bool() {
                    if let Flow::Return(v) = self.exec_block(body)? {
                        return Ok(Flow::Return(v));
                    }
                }
                Ok(Flow::Next)
            }
            Stmt::Block(b) => self.exec_block(b),
        }
    }

    fn eval(&mut self, expr: &Expr) -> Result<Value, RuntimeError> {
        match expr {
            Expr::IntLit { value, .. } => Ok(Value::Int(*value)),
            Expr::BoolLit { value, .. } => Ok(Value::Bool(*value)),
            Expr::StringLit { value, .. } => Ok(Value::Str(value.clone())),
            Expr::Ident { name, span } => self.lookup(name, *span),
            Expr::Unary { op, expr, span } => {
                let v = self.eval(expr)?;
                match (op, v) {
                    (UnaryOp::Neg, Value::Int(n)) => Ok(Value::Int(-n)),
                    (UnaryOp::Not, Value::Bool(b)) => Ok(Value::Bool(!b)),
                    _ => Err(self.err("invalid unary operand", *span)),
                }
            }
            Expr::Binary {
                op,
                left,
                right,
                span,
            } => self.eval_binary(*op, left, right, *span),
            Expr::Assign { name, value, span } => {
                let v = self.eval(value)?;
                self.assign(name, v.clone(), *span)?;
                Ok(v)
            }
            Expr::Call {
                callee,
                args,
                span,
            } => self.eval_call(callee, args, *span),
        }
    }

    fn eval_binary(
        &mut self,
        op: BinaryOp,
        left: &Expr,
        right: &Expr,
        span: Span,
    ) -> Result<Value, RuntimeError> {
        // Short-circuit
        if matches!(op, BinaryOp::And) {
            let l = self.eval(left)?;
            if !l.as_bool() {
                return Ok(Value::Bool(false));
            }
            return Ok(Value::Bool(self.eval(right)?.as_bool()));
        }
        if matches!(op, BinaryOp::Or) {
            let l = self.eval(left)?;
            if l.as_bool() {
                return Ok(Value::Bool(true));
            }
            return Ok(Value::Bool(self.eval(right)?.as_bool()));
        }

        let l = self.eval(left)?;
        let r = self.eval(right)?;
        match (op, l, r) {
            (BinaryOp::Add, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a + b)),
            (BinaryOp::Sub, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a - b)),
            (BinaryOp::Mul, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a * b)),
            (BinaryOp::Div, Value::Int(a), Value::Int(b)) => {
                if b == 0 {
                    Err(self.err("division by zero", span))
                } else {
                    Ok(Value::Int(a / b))
                }
            }
            (BinaryOp::Rem, Value::Int(a), Value::Int(b)) => {
                if b == 0 {
                    Err(self.err("modulo by zero", span))
                } else {
                    Ok(Value::Int(a % b))
                }
            }
            (BinaryOp::Lt, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a < b)),
            (BinaryOp::Le, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a <= b)),
            (BinaryOp::Gt, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a > b)),
            (BinaryOp::Ge, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a >= b)),
            (BinaryOp::Eq, a, b) => Ok(Value::Bool(a == b)),
            (BinaryOp::Ne, a, b) => Ok(Value::Bool(a != b)),
            _ => Err(self.err("invalid binary operands", span)),
        }
    }

    fn eval_call(
        &mut self,
        callee: &str,
        args: &[Expr],
        span: Span,
    ) -> Result<Value, RuntimeError> {
        let mut values = Vec::with_capacity(args.len());
        for a in args {
            values.push(self.eval(a)?);
        }

        if callee == "print" {
            let v = values.into_iter().next().unwrap_or(Value::Void);
            writeln!(self.out, "{v}").map_err(|e| {
                self.err(format!("failed to write output: {e}"), span)
            })?;
            return Ok(Value::Void);
        }

        let func = self
            .functions
            .get(callee)
            .copied()
            .ok_or_else(|| self.err(format!("undefined function `{callee}`"), span))?;

        // New call frame: fresh env with only param scope
        let saved = std::mem::replace(&mut self.env, vec![HashMap::new()]);
        for (param, val) in func.params.iter().zip(values) {
            self.declare(param.name.clone(), val);
        }
        let flow = self.exec_block(&func.body)?;
        self.env = saved;

        match flow {
            Flow::Return(v) => Ok(v),
            Flow::Next => Ok(Value::Void),
        }
    }
}

impl Value {
    fn as_bool(&self) -> bool {
        match self {
            Value::Bool(b) => *b,
            _ => false,
        }
    }
}
