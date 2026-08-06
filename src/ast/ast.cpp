#include "tiny/ast.hpp"

namespace tiny {

ExprUnary::ExprUnary(const ExprUnary& other)
    : op(other.op),
      expr(other.expr ? std::make_unique<Expr>(*other.expr) : nullptr) {}

ExprUnary& ExprUnary::operator=(const ExprUnary& other) {
    if (this != &other) {
        op = other.op;
        expr = other.expr ? std::make_unique<Expr>(*other.expr) : nullptr;
    }
    return *this;
}

bool ExprUnary::operator==(const ExprUnary& other) const {
    if (op != other.op) return false;
    if (!expr && !other.expr) return true;
    if (!expr || !other.expr) return false;
    return *expr == *other.expr;
}

ExprBinary::ExprBinary(const ExprBinary& other)
    : op(other.op),
      left(other.left ? std::make_unique<Expr>(*other.left) : nullptr),
      right(other.right ? std::make_unique<Expr>(*other.right) : nullptr) {}

ExprBinary& ExprBinary::operator=(const ExprBinary& other) {
    if (this != &other) {
        op = other.op;
        left = other.left ? std::make_unique<Expr>(*other.left) : nullptr;
        right = other.right ? std::make_unique<Expr>(*other.right) : nullptr;
    }
    return *this;
}

bool ExprBinary::operator==(const ExprBinary& other) const {
    if (op != other.op) return false;
    if (!left || !other.left || !right || !other.right) return false;
    return (*left == *other.left) && (*right == *other.right);
}

ExprAssign::ExprAssign(const ExprAssign& other)
    : name(other.name),
      value(other.value ? std::make_unique<Expr>(*other.value) : nullptr) {}

ExprAssign& ExprAssign::operator=(const ExprAssign& other) {
    if (this != &other) {
        name = other.name;
        value = other.value ? std::make_unique<Expr>(*other.value) : nullptr;
    }
    return *this;
}

bool ExprAssign::operator==(const ExprAssign& other) const {
    if (name != other.name) return false;
    if (!value && !other.value) return true;
    if (!value || !other.value) return false;
    return *value == *other.value;
}

bool ExprCall::operator==(const ExprCall& other) const {
    return (callee == other.callee) && (args == other.args);
}

bool Expr::operator==(const Expr& other) const {
    if (node.index() != other.node.index()) return false;
    return std::visit([&](const auto& this_val) -> bool {
        using T = std::decay_t<decltype(this_val)>;
        if constexpr (std::is_same_v<T, ExprIntLit>) {
            return this_val.value == std::get<ExprIntLit>(other.node).value;
        } else if constexpr (std::is_same_v<T, ExprBoolLit>) {
            return this_val.value == std::get<ExprBoolLit>(other.node).value;
        } else if constexpr (std::is_same_v<T, ExprStringLit>) {
            return this_val.value == std::get<ExprStringLit>(other.node).value;
        } else if constexpr (std::is_same_v<T, ExprIdent>) {
            return this_val.name == std::get<ExprIdent>(other.node).name;
        } else if constexpr (std::is_same_v<T, ExprUnary>) {
            return this_val == std::get<ExprUnary>(other.node);
        } else if constexpr (std::is_same_v<T, ExprBinary>) {
            return this_val == std::get<ExprBinary>(other.node);
        } else if constexpr (std::is_same_v<T, ExprAssign>) {
            return this_val == std::get<ExprAssign>(other.node);
        } else if constexpr (std::is_same_v<T, ExprCall>) {
            return this_val == std::get<ExprCall>(other.node);
        }
        return false;
    }, node);
}

bool Block::operator==(const Block& other) const {
    return statements == other.statements;
}

bool FnDecl::operator==(const FnDecl& other) const {
    return (name == other.name) &&
           (params == other.params) &&
           (return_type == other.return_type) &&
           (body == other.body);
}

} // namespace tiny
