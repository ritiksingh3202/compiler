//! Constant folding and dead-code elimination.

use crate::ir::{Instr, IrFunction, Module, Reg};
use std::collections::{HashMap, HashSet};

/// Run constant folding then DCE on every function in the module.
pub fn optimize(module: &mut Module) {
    for f in &mut module.functions {
        optimize_function(f);
    }
    optimize_function(&mut module.main);
}

fn optimize_function(func: &mut IrFunction) {
    fold_constants(func);
    eliminate_dead(func);
}

fn fold_constants(func: &mut IrFunction) {
    let mut consts: HashMap<Reg, i64> = HashMap::new();
    let mut out = Vec::with_capacity(func.instructions.len());

    for instr in std::mem::take(&mut func.instructions) {
        let replacement = match &instr {
            Instr::ConstInt(d, v) => {
                consts.insert(*d, *v);
                None
            }
            Instr::ConstBool(d, v) => {
                consts.insert(*d, if *v { 1 } else { 0 });
                None
            }
            Instr::Mov(d, s) => {
                if let Some(v) = consts.get(s).copied() {
                    consts.insert(*d, v);
                    Some(Instr::ConstInt(*d, v))
                } else {
                    consts.remove(d);
                    None
                }
            }
            Instr::Add(d, a, b) => apply_fold(*d, fold_binop(*d, a, b, &consts, |x, y| x.wrapping_add(y)), &mut consts),
            Instr::Sub(d, a, b) => apply_fold(*d, fold_binop(*d, a, b, &consts, |x, y| x.wrapping_sub(y)), &mut consts),
            Instr::Mul(d, a, b) => apply_fold(*d, fold_binop(*d, a, b, &consts, |x, y| x.wrapping_mul(y)), &mut consts),
            Instr::Neg(d, a) => {
                if let Some(v) = consts.get(a).copied() {
                    let n = v.wrapping_neg();
                    consts.insert(*d, n);
                    Some(Instr::ConstInt(*d, n))
                } else {
                    consts.remove(d);
                    None
                }
            }
            Instr::Label(_) | Instr::Jump(_) | Instr::JumpIfFalse(_, _) => {
                // Control flow: clear const map (conservative)
                consts.clear();
                None
            }
            other => {
                // Kill dest if present
                if let Some(d) = instr_dest(other) {
                    consts.remove(&d);
                }
                None
            }
        };

        out.push(replacement.unwrap_or(instr));
    }
    func.instructions = out;
}

fn fold_binop(
    d: Reg,
    a: &Reg,
    b: &Reg,
    consts: &HashMap<Reg, i64>,
    f: impl Fn(i64, i64) -> i64,
) -> Option<Instr> {
    match (consts.get(a), consts.get(b)) {
        (Some(&x), Some(&y)) => Some(Instr::ConstInt(d, f(x, y))),
        _ => None,
    }
}

fn apply_fold(
    d: Reg,
    folded: Option<Instr>,
    consts: &mut HashMap<Reg, i64>,
) -> Option<Instr> {
    match folded {
        Some(Instr::ConstInt(reg, v)) => {
            consts.insert(reg, v);
            Some(Instr::ConstInt(reg, v))
        }
        Some(other) => Some(other),
        None => {
            consts.remove(&d);
            None
        }
    }
}

fn instr_dest(instr: &Instr) -> Option<Reg> {
    match instr {
        Instr::ConstInt(d, _)
        | Instr::ConstBool(d, _)
        | Instr::ConstStr(d, _)
        | Instr::Mov(d, _)
        | Instr::Add(d, _, _)
        | Instr::Sub(d, _, _)
        | Instr::Mul(d, _, _)
        | Instr::Div(d, _, _)
        | Instr::Mod(d, _, _)
        | Instr::Neg(d, _)
        | Instr::Not(d, _)
        | Instr::Cmp(_, d, _, _) => Some(*d),
        Instr::Call { dest, .. } => *dest,
        _ => None,
    }
}

/// Remove instructions after an unconditional Return until next Label;
/// remove unused pure computations (simple single-pass liveness from uses).
fn eliminate_dead(func: &mut IrFunction) {
    // Drop unreachable after return
    let mut out = Vec::new();
    let mut dead = false;
    for instr in std::mem::take(&mut func.instructions) {
        if matches!(instr, Instr::Label(_)) {
            dead = false;
        }
        if dead {
            continue;
        }
        if matches!(instr, Instr::Return(_)) {
            out.push(instr);
            dead = true;
            continue;
        }
        out.push(instr);
    }

    // Collect used regs (backward)
    let mut used: HashSet<Reg> = HashSet::new();
    // Anything that escapes: return, print, call args, jump cond
    for instr in out.iter().rev() {
        match instr {
            Instr::Return(Some(r)) | Instr::Print(r, _) | Instr::JumpIfFalse(r, _) => {
                used.insert(*r);
            }
            Instr::Call { args, dest, .. } => {
                for a in args {
                    used.insert(*a);
                }
                if let Some(d) = dest {
                    // dest is produced; if never used later it's dead — handled by used set
                    let _ = d;
                }
            }
            Instr::Mov(d, s) => {
                if used.contains(d) {
                    used.insert(*s);
                }
            }
            Instr::Add(d, a, b)
            | Instr::Sub(d, a, b)
            | Instr::Mul(d, a, b)
            | Instr::Div(d, a, b)
            | Instr::Mod(d, a, b)
            | Instr::Cmp(_, d, a, b) => {
                if used.contains(d) {
                    used.insert(*a);
                    used.insert(*b);
                }
            }
            Instr::Neg(d, a) | Instr::Not(d, a) => {
                if used.contains(d) {
                    used.insert(*a);
                }
            }
            Instr::ConstInt(d, _) | Instr::ConstBool(d, _) | Instr::ConstStr(d, _) => {
                let _ = d; // keep if used — filtered below
            }
            _ => {}
        }
    }

    // Forward pass: also mark call dests as needed if used
    // Rebuild used properly with iterative backward scan
    used.clear();
    for instr in out.iter().rev() {
        mark_uses(instr, &mut used);
    }

    func.instructions = out
        .into_iter()
        .filter(|instr| keep_instr(instr, &used))
        .collect();
}

fn mark_uses(instr: &Instr, used: &mut HashSet<Reg>) {
    match instr {
        Instr::Return(Some(r)) | Instr::Print(r, _) | Instr::JumpIfFalse(r, _) => {
            used.insert(*r);
        }
        Instr::Call { args, dest, .. } => {
            for a in args {
                used.insert(*a);
            }
            // Call has side effects — always keep; dest may feed used
            if let Some(d) = dest {
                if !used.contains(d) {
                    // still keep call; dest unused is fine
                }
            }
        }
        Instr::Mov(d, s) => {
            if used.contains(d) {
                used.insert(*s);
            }
        }
        Instr::Add(d, a, b)
        | Instr::Sub(d, a, b)
        | Instr::Mul(d, a, b)
        | Instr::Div(d, a, b)
        | Instr::Mod(d, a, b)
        | Instr::Cmp(_, d, a, b) => {
            if used.contains(d) {
                used.insert(*a);
                used.insert(*b);
            }
        }
        Instr::Neg(d, a) | Instr::Not(d, a) => {
            if used.contains(d) {
                used.insert(*a);
            }
        }
        _ => {}
    }
}

fn keep_instr(instr: &Instr, used: &HashSet<Reg>) -> bool {
    match instr {
        Instr::Jump(_)
        | Instr::JumpIfFalse(_, _)
        | Instr::Label(_)
        | Instr::Return(_)
        | Instr::Print(_, _)
        | Instr::Call { .. } => true,
        Instr::ConstInt(d, _)
        | Instr::ConstBool(d, _)
        | Instr::ConstStr(d, _)
        | Instr::Mov(d, _)
        | Instr::Add(d, _, _)
        | Instr::Sub(d, _, _)
        | Instr::Mul(d, _, _)
        | Instr::Div(d, _, _)
        | Instr::Mod(d, _, _)
        | Instr::Neg(d, _)
        | Instr::Not(d, _)
        | Instr::Cmp(_, d, _, _) => used.contains(d),
    }
}
