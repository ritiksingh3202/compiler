#pragma once

#include "tiny/span.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>

namespace tiny {

enum class TypeKind {
    Int,
    Bool,
    String,
    Void
};

struct Type {
    TypeKind kind{TypeKind::Void};

    constexpr Type() = default;
    constexpr Type(TypeKind k) : kind(k) {}

    static constexpr Type Int() { return Type(TypeKind::Int); }
    static constexpr Type Bool() { return Type(TypeKind::Bool); }
    static constexpr Type String() { return Type(TypeKind::String); }
    static constexpr Type Void() { return Type(TypeKind::Void); }

    bool operator==(const Type& other) const = default;
    bool operator!=(const Type& other) const = default;

    std::string to_string() const {
        switch (kind) {
            case TypeKind::Int: return "int";
            case TypeKind::Bool: return "bool";
            case TypeKind::String: return "string";
            case TypeKind::Void: return "void";
        }
        return "unknown";
    }
};

enum class UnaryOp {
    Neg,
    Not
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or
};

struct Expr;
