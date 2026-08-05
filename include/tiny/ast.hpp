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

struct ExprIntLit { int64_t value; };
struct ExprBoolLit { bool value; };
struct ExprStringLit { std::string value; };
struct ExprIdent { std::string name; };
struct ExprUnary {
    UnaryOp op;
    std::unique_ptr<Expr> expr;
    ExprUnary(UnaryOp o, std::unique_ptr<Expr> e) : op(o), expr(std::move(e)) {}
    ExprUnary(const ExprUnary& other);
    ExprUnary& operator=(const ExprUnary& other);
    bool operator==(const ExprUnary& other) const;
};
struct ExprBinary {
    BinaryOp op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    ExprBinary(BinaryOp o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    ExprBinary(const ExprBinary& other);
    ExprBinary& operator=(const ExprBinary& other);
    bool operator==(const ExprBinary& other) const;
};
struct ExprAssign {
    std::string name;
    std::unique_ptr<Expr> value;
    ExprAssign(std::string n, std::unique_ptr<Expr> v) : name(std::move(n)), value(std::move(v)) {}
    ExprAssign(const ExprAssign& other);
    ExprAssign& operator=(const ExprAssign& other);
    bool operator==(const ExprAssign& other) const;
};
struct ExprCall {
    std::string callee;
    std::vector<Expr> args;
    bool operator==(const ExprCall& other) const;
};

using ExprNode = std::variant<
    ExprIntLit,
    ExprBoolLit,
    ExprStringLit,
    ExprIdent,
    ExprUnary,
    ExprBinary,
    ExprAssign,
    ExprCall
>;

struct Expr {
    ExprNode node;
    Span span_;

    Expr() : node(ExprIntLit{0}), span_(Span::empty(Position::start())) {}
    Expr(ExprNode n, Span s) : node(std::move(n)), span_(s) {}
    Expr(const Expr& other) = default;
    Expr(Expr&& other) noexcept = default;
    Expr& operator=(const Expr& other) = default;
    Expr& operator=(Expr&& other) noexcept = default;

    Span span() const { return span_; }

    static Expr make_int(int64_t val, Span span) {
        return Expr(ExprIntLit{val}, span);
    }
    static Expr make_bool(bool val, Span span) {
        return Expr(ExprBoolLit{val}, span);
    }
    static Expr make_string(std::string val, Span span) {
        return Expr(ExprStringLit{std::move(val)}, span);
    }
    static Expr make_ident(std::string name, Span span) {
        return Expr(ExprIdent{std::move(name)}, span);
    }
    static Expr make_unary(UnaryOp op, Expr expr, Span span) {
        return Expr(ExprUnary(op, std::make_unique<Expr>(std::move(expr))), span);
    }
    static Expr make_binary(BinaryOp op, Expr left, Expr right, Span span) {
        return Expr(ExprBinary(op, std::make_unique<Expr>(std::move(left)), std::make_unique<Expr>(std::move(right))), span);
    }
    static Expr make_assign(std::string name, Expr value, Span span) {
        return Expr(ExprAssign(std::move(name), std::make_unique<Expr>(std::move(value))), span);
    }
    static Expr make_call(std::string callee, std::vector<Expr> args, Span span) {
        return Expr(ExprCall{std::move(callee), std::move(args)}, span);
    }

    bool operator==(const Expr& other) const;
};

// Forward declaration of Stmt
struct Stmt;

struct Block {
    std::vector<Stmt> statements;
    Span span;

    bool operator==(const Block& other) const;
};

struct Param {
    std::string name;
    Type ty;
    Span span;

    bool operator==(const Param& other) const = default;
};

struct FnDecl {
    std::string name;
    std::vector<Param> params;
    Type return_type{TypeKind::Void};
    Block body;
    Span span;

    bool operator==(const FnDecl& other) const;
};

struct StmtLet {
    std::string name;
    std::optional<Type> ty;
    Expr init;
    bool operator==(const StmtLet& other) const = default;
};

struct StmtExpr {
    Expr expr;
    bool operator==(const StmtExpr& other) const = default;
};

struct StmtReturn {
    std::optional<Expr> value;
    bool operator==(const StmtReturn& other) const = default;
};

struct StmtIf {
    Expr cond;
    Block then_block;
    std::optional<Block> else_block;
    bool operator==(const StmtIf& other) const = default;
};

struct StmtWhile {
    Expr cond;
    Block body;
    bool operator==(const StmtWhile& other) const = default;
};

struct StmtBlock {
    Block block;
    bool operator==(const StmtBlock& other) const = default;
};

using StmtNode = std::variant<
    StmtLet,
    StmtExpr,
    StmtReturn,
    StmtIf,
    StmtWhile,
    StmtBlock
>;

struct Stmt {
    StmtNode node;
    Span span_;

    Stmt(StmtNode n, Span s) : node(std::move(n)), span_(s) {}

    Span span() const {
        if (std::holds_alternative<StmtExpr>(node)) {
            return std::get<StmtExpr>(node).expr.span();
        }
        if (std::holds_alternative<StmtBlock>(node)) {
            return std::get<StmtBlock>(node).block.span;
        }
        return span_;
    }

    static Stmt make_let(std::string name, std::optional<Type> ty, Expr init, Span span) {
        return Stmt(StmtLet{std::move(name), ty, std::move(init)}, span);
    }
    static Stmt make_expr(Expr expr) {
        Span s = expr.span();
        return Stmt(StmtExpr{std::move(expr)}, s);
    }
    static Stmt make_return(std::optional<Expr> val, Span span) {
        return Stmt(StmtReturn{std::move(val)}, span);
    }
    static Stmt make_if(Expr cond, Block then_b, std::optional<Block> else_b, Span span) {
        return Stmt(StmtIf{std::move(cond), std::move(then_b), std::move(else_b)}, span);
    }
    static Stmt make_while(Expr cond, Block body, Span span) {
        return Stmt(StmtWhile{std::move(cond), std::move(body)}, span);
    }
    static Stmt make_block(Block block) {
        Span s = block.span;
        return Stmt(StmtBlock{std::move(block)}, s);
    }

    bool operator==(const Stmt& other) const = default;
};

struct Program {
    std::vector<FnDecl> functions;
    std::vector<Stmt> statements;
    Span span;

    bool operator==(const Program& other) const = default;
};

} // namespace tiny
