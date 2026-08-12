//! Function table and lexical scopes.

use std::collections::HashMap;
use tiny_ast::{Param, Type};
use tiny_lexer::Span;

/// A registered function signature (pass 1).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FnSig {
    pub name: String,
    pub params: Vec<(String, Type)>,
    pub return_type: Type,
    pub span: Span,
}

impl FnSig {
    pub fn from_params(name: String, params: &[Param], return_type: Type, span: Span) -> Self {
        Self {
            name,
            params: params
                .iter()
                .map(|p| (p.name.clone(), p.ty.clone()))
                .collect(),
            return_type,
            span,
        }
    }
}

/// Nested scopes: innermost last. Shadowing allowed across levels.
#[derive(Debug, Default)]
pub struct ScopeStack {
    scopes: Vec<HashMap<String, Type>>,
}

impl ScopeStack {
    pub fn new() -> Self {
        Self {
            scopes: vec![HashMap::new()],
        }
    }

    pub fn push(&mut self) {
        self.scopes.push(HashMap::new());
    }

    pub fn pop(&mut self) {
        if self.scopes.len() > 1 {
            self.scopes.pop();
        }
    }

    /// Declare in the current (innermost) scope. Returns previous binding if redeclared here.
    pub fn declare(&mut self, name: String, ty: Type) -> Option<Type> {
        let scope = self.scopes.last_mut()?;
        scope.insert(name, ty)
    }

    pub fn lookup(&self, name: &str) -> Option<&Type> {
        for scope in self.scopes.iter().rev() {
            if let Some(ty) = scope.get(name) {
                return Some(ty);
            }
        }
        None
    }
}
