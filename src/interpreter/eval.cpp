#include "tiny/interp.hpp"

namespace tiny {

namespace {

enum class FlowKind {
    Next,
    Return
};

struct Flow {
    FlowKind kind{FlowKind::Next};
    Value return_val;

    static Flow Next() { return Flow{FlowKind::Next, Value::Void()}; }
    static Flow Return(Value v) { return Flow{FlowKind::Return, std::move(v)}; }
};

using Env = std::vector<std::unordered_map<std::string, Value>>;

class Interpreter {
public:
    Interpreter(const Program& program, std::string_view source, std::ostream& out)
        : program_(program), source_(source), out_(out) {
        for (const auto& f : program_.functions) {
            functions_.emplace(f.name, &f);
        }
        env_.emplace_back();
    }

    std::variant<std::monostate, RuntimeError> run_program() {
        for (const auto& stmt : program_.statements) {
            auto res = exec_stmt(stmt);
            if (std::holds_alternative<RuntimeError>(res)) {
                return std::get<RuntimeError>(res);
            }
            const auto& flow = std::get<Flow>(res);
            if (flow.kind == FlowKind::Return) {
                break;
            }
        }
        return std::monostate{};
    }

private:
    const Program& program_;
    std::string_view source_;
    std::ostream& out_;
    std::unordered_map<std::string, const FnDecl*> functions_;
    Env env_;

    RuntimeError err(std::string message, Span span) {
        return RuntimeError(std::move(message), span, source_);
    }

    void push_scope() {
        env_.emplace_back();
    }

    void pop_scope() {
        if (env_.size() > 1) {
            env_.pop_back();
        }
    }

    void declare(std::string name, Value val) {
        if (!env_.empty()) {
            env_.back().emplace(std::move(name), std::move(val));
        }
    }

    std::optional<RuntimeError> assign(const std::string& name, Value val, Span span) {
        for (auto it = env_.rbegin(); it != env_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second = std::move(val);
                return std::nullopt;
            }
        }
        return err("undefined variable `" + name + "`", span);
    }

    std::variant<Value, RuntimeError> lookup(const std::string& name, Span span) {
        for (auto it = env_.rbegin(); it != env_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return err("undefined variable `" + name + "`", span);
    }

    std::variant<Flow, RuntimeError> exec_block(const Block& block) {
        push_scope();
        Flow flow = Flow::Next();
        for (const auto& stmt : block.statements) {
            auto res = exec_stmt(stmt);
            if (std::holds_alternative<RuntimeError>(res)) {
                pop_scope();
                return res;
            }
            flow = std::get<Flow>(res);
            if (flow.kind == FlowKind::Return) {
                break;
            }
        }
        pop_scope();
        return flow;
    }

    std::variant<Flow, RuntimeError> exec_stmt(const Stmt& stmt) {
        return std::visit([&](const auto& val) -> std::variant<Flow, RuntimeError> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, StmtLet>) {
                auto v_res = eval(val.init);
                if (std::holds_alternative<RuntimeError>(v_res)) return std::get<RuntimeError>(v_res);
                declare(val.name, std::get<Value>(std::move(v_res)));
                return Flow::Next();
            } else if constexpr (std::is_same_v<T, StmtExpr>) {
                auto v_res = eval(val.expr);
                if (std::holds_alternative<RuntimeError>(v_res)) return std::get<RuntimeError>(v_res);
                return Flow::Next();
            } else if constexpr (std::is_same_v<T, StmtReturn>) {
                Value v = Value::Void();
                if (val.value) {
                    auto v_res = eval(*val.value);
                    if (std::holds_alternative<RuntimeError>(v_res)) return std::get<RuntimeError>(v_res);
                    v = std::get<Value>(std::move(v_res));
                }
                return Flow::Return(std::move(v));
            } else if constexpr (std::is_same_v<T, StmtIf>) {
                auto c_res = eval(val.cond);
                if (std::holds_alternative<RuntimeError>(c_res)) return std::get<RuntimeError>(c_res);
                if (std::get<Value>(c_res).as_bool()) {
                    return exec_block(val.then_block);
                } else if (val.else_block) {
                    return exec_block(*val.else_block);
                }
                return Flow::Next();
            } else if constexpr (std::is_same_v<T, StmtWhile>) {
                while (true) {
                    auto c_res = eval(val.cond);
                    if (std::holds_alternative<RuntimeError>(c_res)) return std::get<RuntimeError>(c_res);
                    if (!std::get<Value>(c_res).as_bool()) {
                        break;
                    }
                    auto res = exec_block(val.body);
                    if (std::holds_alternative<RuntimeError>(res)) return res;
                    const auto& flow = std::get<Flow>(res);
                    if (flow.kind == FlowKind::Return) {
                        return flow;
                    }
                }
                return Flow::Next();
            } else if constexpr (std::is_same_v<T, StmtBlock>) {
                return exec_block(val.block);
            }
            return Flow::Next();
        }, stmt.node);
    }

    std::variant<Value, RuntimeError> eval(const Expr& expr) {
        return std::visit([&](const auto& val) -> std::variant<Value, RuntimeError> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, ExprIntLit>) {
                return Value::Int(val.value);
            } else if constexpr (std::is_same_v<T, ExprBoolLit>) {
                return Value::Bool(val.value);
            } else if constexpr (std::is_same_v<T, ExprStringLit>) {
                return Value::Str(val.value);
            } else if constexpr (std::is_same_v<T, ExprIdent>) {
                return lookup(val.name, expr.span());
            } else if constexpr (std::is_same_v<T, ExprUnary>) {
                auto v_res = eval(*val.expr);
                if (std::holds_alternative<RuntimeError>(v_res)) return v_res;
                Value v = std::get<Value>(v_res);

                if (val.op == UnaryOp::Neg && v.kind == ValueKind::Int) {
                    return Value::Int(-v.as_int());
                } else if (val.op == UnaryOp::Not && v.kind == ValueKind::Bool) {
                    return Value::Bool(!v.as_bool());
                }
                return err("invalid unary operand", expr.span());
            } else if constexpr (std::is_same_v<T, ExprBinary>) {
                return eval_binary(val.op, *val.left, *val.right, expr.span());
            } else if constexpr (std::is_same_v<T, ExprAssign>) {
                auto v_res = eval(*val.value);
                if (std::holds_alternative<RuntimeError>(v_res)) return v_res;
                Value v = std::get<Value>(v_res);
                if (auto e = assign(val.name, v, expr.span())) {
                    return *e;
                }
                return v;
            } else if constexpr (std::is_same_v<T, ExprCall>) {
                return eval_call(val.callee, val.args, expr.span());
            }
            return Value::Void();
        }, expr.node);
    }

    std::variant<Value, RuntimeError> eval_binary(BinaryOp op, const Expr& left, const Expr& right, Span span) {
        if (op == BinaryOp::And) {
            auto l_res = eval(left);
            if (std::holds_alternative<RuntimeError>(l_res)) return l_res;
            if (!std::get<Value>(l_res).as_bool()) {
                return Value::Bool(false);
            }
            auto r_res = eval(right);
            if (std::holds_alternative<RuntimeError>(r_res)) return r_res;
            return Value::Bool(std::get<Value>(r_res).as_bool());
        }
        if (op == BinaryOp::Or) {
            auto l_res = eval(left);
            if (std::holds_alternative<RuntimeError>(l_res)) return l_res;
            if (std::get<Value>(l_res).as_bool()) {
                return Value::Bool(true);
            }
            auto r_res = eval(right);
            if (std::holds_alternative<RuntimeError>(r_res)) return r_res;
            return Value::Bool(std::get<Value>(r_res).as_bool());
        }

        auto l_res = eval(left);
        if (std::holds_alternative<RuntimeError>(l_res)) return l_res;
        auto r_res = eval(right);
        if (std::holds_alternative<RuntimeError>(r_res)) return r_res;

        Value l = std::get<Value>(l_res);
        Value r = std::get<Value>(r_res);

        switch (op) {
            case BinaryOp::Add:
                return Value::Int(l.as_int() + r.as_int());
            case BinaryOp::Sub:
                return Value::Int(l.as_int() - r.as_int());
            case BinaryOp::Mul:
                return Value::Int(l.as_int() * r.as_int());
            case BinaryOp::Div:
                if (r.as_int() == 0) return err("division by zero", span);
                return Value::Int(l.as_int() / r.as_int());
            case BinaryOp::Rem:
                if (r.as_int() == 0) return err("modulo by zero", span);
                return Value::Int(l.as_int() % r.as_int());
            case BinaryOp::Lt:
                return Value::Bool(l.as_int() < r.as_int());
            case BinaryOp::Le:
                return Value::Bool(l.as_int() <= r.as_int());
            case BinaryOp::Gt:
                return Value::Bool(l.as_int() > r.as_int());
            case BinaryOp::Ge:
                return Value::Bool(l.as_int() >= r.as_int());
            case BinaryOp::Eq:
                return Value::Bool(l == r);
            case BinaryOp::Ne:
                return Value::Bool(!(l == r));
            default:
                return err("invalid binary operands", span);
        }
    }

    std::variant<Value, RuntimeError> eval_call(const std::string& callee, const std::vector<Expr>& args, Span span) {
        std::vector<Value> values;
        for (const auto& a : args) {
            auto v_res = eval(a);
            if (std::holds_alternative<RuntimeError>(v_res)) return v_res;
            values.push_back(std::get<Value>(std::move(v_res)));
        }

        if (callee == "print") {
            Value v = values.empty() ? Value::Void() : values[0];
            out_ << v.to_string() << "\n";
            return Value::Void();
        }

        auto it = functions_.find(callee);
        if (it == functions_.end()) {
            return err("undefined function `" + callee + "`", span);
        }
        const FnDecl* func = it->second;

        Env saved_env = std::move(env_);
        env_ = Env{std::unordered_map<std::string, Value>{}};

        for (size_t i = 0; i < func->params.size() && i < values.size(); ++i) {
            declare(func->params[i].name, std::move(values[i]));
        }

        auto res = exec_block(func->body);
        env_ = std::move(saved_env);

        if (std::holds_alternative<RuntimeError>(res)) {
            return std::get<RuntimeError>(res);
        }

        const Flow& flow = std::get<Flow>(res);
        if (flow.kind == FlowKind::Return) {
            return flow.return_val;
        }
        return Value::Void();
    }
};

} // namespace

std::variant<std::monostate, RuntimeError> run(const Program& program, std::string_view source, std::ostream& out) {
    Interpreter interp(program, source, out);
    return interp.run_program();
}

std::variant<std::string, RuntimeError> run_to_string(const Program& program, std::string_view source) {
    std::ostringstream oss;
    auto res = run(program, source, oss);
    if (std::holds_alternative<RuntimeError>(res)) {
        return std::get<RuntimeError>(res);
    }
    return oss.str();
}

} // namespace tiny
