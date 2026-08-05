//! Type annotations written in Tiny source.

/// A type as it appears in annotations, plus [`Void`] for functions with no return.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Type {
    Int,
    Bool,
    String,
    /// Functions with no `->` return type; also the type of `print(...)`.
    Void,
}

impl std::fmt::Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Int => write!(f, "int"),
            Self::Bool => write!(f, "bool"),
            Self::String => write!(f, "string"),
            Self::Void => write!(f, "void"),
        }
    }
}
