#include "tiny/sema.hpp"
#include <unordered_set>

namespace tiny {

namespace {

enum class TyKind {
    Known,
    Error
};

struct Ty {
    TyKind kind;
    Type type;

    static Ty Known(Type t) { return Ty{TyKind::Known, t}; }
    static Ty Error() { return Ty{TyKind::Error, Type::Void()}; }

    const Type* as_known() const {
        if (kind == TyKind::Known) return &type;
        return nullptr;
    }
};

bool stmt_always_returns(const Stmt& stmt);

bool block_always_returns(const Block& block) {
    for (const auto& s : block.statements) {
        if (stmt_always_returns(s)) return true;
    }
    return false;
}

bool stmt_always_returns(const Stmt& stmt) {
    return std::visit([&](const auto& val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, StmtReturn>) {
            return true;
        } else if constexpr (std::is_same_v<T, StmtIf>) {
            if (val.else_block) {
                return block_always_returns(val.then_block) && block_always_returns(*val.else_block);
            }
            return false;
        } else if constexpr (std::is_same_v<T, StmtBlock>) {
            return block_always_returns(val.block);
        } else {
            return false;
        }
    }, stmt.node);
}

class Checker {
public:
    explicit Checker(std::string_view source)
        : source_(source), expected_return_(Type::Void()) {}

    void error(std::string message, Span span) {
        errors_.emplace_back(std::move(message), span, source_);
    }

    void collect_functions(const Program& program) {
        for (const auto& func : program.functions) {
            if (functions_.find(func.name) != functions_.end()) {
                error("function `" + func.name + "` is already defined", func.span);
                continue;
            }

            std::unordered_set<std::string> seen;
            for (const auto& p : func.params) {
                if (!seen.insert(p.name).second) {
                    error("duplicate parameter `" + p.name + "` in function `" + func.name + "`", p.span);
                }
                if (p.ty == Type::Void()) {
                    error("parameters cannot have type `void`", p.span);
                }
            }

            functions_.emplace(func.name, FnSig::from_params(func.name, func.params, func.return_type, func.span));
        }
    }

    void check_program(const Program& program) {
        for (const auto& func : program.functions) {
            check_function(func);
        }

        expected_return_ = Type::Void();
        scopes_ = ScopeStack();
        for (const auto& stmt : program.statements) {
            check_stmt(stmt);
        }
    }

    std::vector<SemaError> take_errors() {
        return std::move(errors_);
    }

private:
    std::string_view source_;
    std::unordered_map<std::string, FnSig> functions_;
    ScopeStack scopes_;
    Type expected_return_;
    std::vector<SemaError> errors_;

    void check_function(const FnDecl& func) {
        expected_return_ = func.return_type;
        scopes_ = ScopeStack();
        scopes_.push();

        for (const auto& p : func.params) {
            scopes_.declare(p.name, p.ty);
        }

        check_block(func.body, false);

        if (func.return_type != Type::Void() && !block_always_returns(func.body)) {
            error("function `" + func.name + "` must return a value of type `" +
                  func.return_type.to_string() + "` on all paths", func.span);
        }

        scopes_.pop();
    }

    void check_block(const Block& block, bool new_scope) {
        if (new_scope) {
            scopes_.push();
        }
        for (const auto& stmt : block.statements) {
            check_stmt(stmt);
        }
        if (new_scope) {
            scopes_.pop();
        }
    }

    void check_stmt(const Stmt& stmt) {
        std::visit([&](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, StmtLet>) {
                Ty init_ty = check_expr(val.init);
                Ty bound = Ty::Error();

                const Type* got = init_ty.as_known();
                if (val.ty.has_value()) {
                    Type ann = *val.ty;
                    if (ann == Type::Void()) {
                        error("cannot declare a variable of type `void`", stmt.span());
                        bound = Ty::Error();
                    } else if (got && ann != *got) {
                        error("type mismatch in `let`: expected `" + ann.to_string() +
                              "`, found `" + got->to_string() + "`", val.init.span());
                        bound = Ty::Known(ann);
                    } else {
                        bound = Ty::Known(ann);
                    }
                } else {
                    if (got && *got == Type::Void()) {
                        error("cannot infer type of `void` expression in `let`", val.init.span());
                        bound = Ty::Error();
                    } else if (got) {
                        bound = Ty::Known(*got);
                    } else {
                        bound = Ty::Error();
                    }
                }

                if (const Type* t = bound.as_known()) {
                    if (scopes_.declare(val.name, *t).has_value()) {
                        error("variable `" + val.name + "` is already declared in this scope", stmt.span());
                    }
                }
            } else if constexpr (std::is_same_v<T, StmtExpr>) {
                check_expr(val.expr);
            } else if constexpr (std::is_same_v<T, StmtReturn>) {
                Type expected = expected_return_;
                if (!val.value) {
                    if (expected != Type::Void()) {
                        error("missing return value: expected `" + expected.to_string() + "`", stmt.span());
                    }
                } else {
                    if (expected == Type::Void()) {
                        check_expr(*val.value);
                        error("cannot return a value from a `void` function", val.value->span());
                    } else {
                        Ty got = check_expr(*val.value);
                        if (const Type* g = got.as_known()) {
                            if (*g != expected) {
                                error("return type mismatch: expected `" + expected.to_string() +
                                      "`, found `" + g->to_string() + "`", val.value->span());
                            }
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, StmtIf>) {
                expect_bool(val.cond, "if condition");
                check_block(val.then_block, true);
                if (val.else_block) {
                    check_block(*val.else_block, true);
                }
            } else if constexpr (std::is_same_v<T, StmtWhile>) {
                expect_bool(val.cond, "while condition");
                check_block(val.body, true);
            } else if constexpr (std::is_same_v<T, StmtBlock>) {
                check_block(val.block, true);
            }
        }, stmt.node);
    }

    void expect_bool(const Expr& expr, std::string_view what) {
        Ty ty = check_expr(expr);
        if (const Type* t = ty.as_known()) {
            if (*t != Type::Bool()) {
                error(std::string(what) + " must be `bool`, found `" + t->to_string() + "`", expr.span());
            }
        }
    }

    Ty check_expr(const Expr& expr) {
        return std::visit([&](const auto& val) -> Ty {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, ExprIntLit>) {
                return Ty::Known(Type::Int());
            } else if constexpr (std::is_same_v<T, ExprStringLit>) {
                return Ty::Known(Type::String());
            } else if constexpr (std::is_same_v<T, ExprBoolLit>) {
                return Ty::Known(Type::Bool());
            } else if constexpr (std::is_same_v<T, ExprIdent>) {
                const Type* ty = scopes_.lookup(val.name);
                if (ty) {
                    return Ty::Known(*ty);
                }
                error("undefined variable `" + val.name + "`", expr.span());
                return Ty::Error();
            } else if constexpr (std::is_same_v<T, ExprUnary>) {
                Ty inner_ty = check_expr(*val.expr);
                const Type* t = inner_ty.as_known();
                if (!t) return Ty::Error();

                if (val.op == UnaryOp::Neg) {
                    if (*t == Type::Int()) return Ty::Known(Type::Int());
                    error("unary `-` requires `int`, found `" + t->to_string() + "`", expr.span());
                    return Ty::Error();
                } else {
                    if (*t == Type::Bool()) return Ty::Known(Type::Bool());
                    error("unary `!` requires `bool`, found `" + t->to_string() + "`", expr.span());
                    return Ty::Error();
                }
            } else if constexpr (std::is_same_v<T, ExprBinary>) {
                return check_binary(val.op, *val.left, *val.right, expr.span());
            } else if constexpr (std::is_same_v<T, ExprAssign>) {
                Ty value_ty = check_expr(*val.value);
                const Type* expected = scopes_.lookup(val.name);
                if (!expected) {
                    error("undefined variable `" + val.name + "`", expr.span());
                    return Ty::Error();
                }
                if (const Type* got = value_ty.as_known()) {
                    if (*got != *expected) {
                        error("cannot assign `" + got->to_string() + "` to `" + val.name +
                              "` of type `" + expected->to_string() + "`", val.value->span());
                    }
                }
                return Ty::Known(*expected);
            } else if constexpr (std::is_same_v<T, ExprCall>) {
                return check_call(val.callee, val.args, expr.span());
            }
            return Ty::Error();
        }, expr.node);
    }

    Ty check_binary(BinaryOp op, const Expr& left, const Expr& right, Span span) {
        Ty lt = check_expr(left);
        Ty rt = check_expr(right);
        const Type* l = lt.as_known();
        const Type* r = rt.as_known();
        if (!l || !r) return Ty::Error();

        switch (op) {
            case BinaryOp::Add:
            case BinaryOp::Sub:
            case BinaryOp::Mul:
            case BinaryOp::Div:
            case BinaryOp::Rem: {
                if (*l == Type::Int() && *r == Type::Int()) {
                    return Ty::Known(Type::Int());
                }
                error("arithmetic operator requires `int` operands, found `" +
                      l->to_string() + "` and `" + r->to_string() + "`", span);
                return Ty::Error();
            }
            case BinaryOp::Lt:
            case BinaryOp::Le:
            case BinaryOp::Gt:
            case BinaryOp::Ge: {
                if (*l == Type::Int() && *r == Type::Int()) {
                    return Ty::Known(Type::Bool());
                }
                error("comparison requires `int` operands, found `" +
                      l->to_string() + "` and `" + r->to_string() + "`", span);
                return Ty::Error();
            }
            case BinaryOp::Eq:
            case BinaryOp::Ne: {
                if (*l == *r && (*l == Type::Int() || *l == Type::Bool() || *l == Type::String())) {
                    return Ty::Known(Type::Bool());
                }
                error("cannot compare `" + l->to_string() + "` and `" + r->to_string() + "` with `==`/`!=`", span);
                return Ty::Error();
            }
            case BinaryOp::And:
            case BinaryOp::Or: {
                if (*l == Type::Bool() && *r == Type::Bool()) {
                    return Ty::Known(Type::Bool());
                }
                error("logical operator requires `bool` operands, found `" +
                      l->to_string() + "` and `" + r->to_string() + "`", span);
                return Ty::Error();
            }
        }
        return Ty::Error();
    }

    Ty check_call(const std::string& callee, const std::vector<Expr>& args, Span span) {
        if (callee == "print") {
            if (args.size() != 1) {
                error("`print` takes 1 argument, found " + std::to_string(args.size()), span);
                for (const auto& a : args) {
                    check_expr(a);
                }
                return Ty::Error();
            }
            Ty arg_ty = check_expr(args[0]);
            if (const Type* t = arg_ty.as_known()) {
                if (*t == Type::Int() || *t == Type::Bool() || *t == Type::String()) {
                    return Ty::Known(Type::Void());
                }
                error("`print` argument must be `int`, `bool`, or `string`, found `" + t->to_string() + "`", args[0].span());
                return Ty::Error();
            }
            return Ty::Error();
        }

        auto it = functions_.find(callee);
        if (it == functions_.end()) {
            error("undefined function `" + callee + "`", span);
            for (const auto& a : args) {
                check_expr(a);
            }
            return Ty::Error();
        }

        const FnSig& sig = it->second;
        if (args.size() != sig.params.size()) {
            error("function `" + callee + "` takes " + std::to_string(sig.params.size()) +
                  " argument(s), found " + std::to_string(args.size()), span);
        }

        for (size_t i = 0; i < args.size(); ++i) {
            Ty got = check_expr(args[i]);
            if (i < sig.params.size()) {
                Type expected = sig.params[i].second;
                if (const Type* g = got.as_known()) {
                    if (*g != expected) {
                        error("argument " + std::to_string(i + 1) + " to `" + callee + "`: expected `" +
                              expected.to_string() + "`, found `" + g->to_string() + "`", args[i].span());
                    }
                }
            }
        }

        return Ty::Known(sig.return_type);
    }
};

} // namespace

std::optional<SemaErrors> check(const Program& program, std::string_view source) {
    Checker checker(source);
    checker.collect_functions(program);
    checker.check_program(program);
    auto errors = checker.take_errors();
    if (errors.empty()) {
        return std::nullopt;
    }
    return SemaErrors{std::move(errors)};
}

} // namespace tiny
