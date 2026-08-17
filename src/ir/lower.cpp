#include "tiny/ir.hpp"
#include <unordered_map>
#include <algorithm>

namespace tiny {

namespace {

class Lower {
public:
    explicit Lower(std::unordered_map<std::string, Type> fn_returns)
        : next_reg_(0), next_label_(0), fn_returns_(std::move(fn_returns)) {
        locals_.emplace_back();
    }

    Reg fresh_reg() {
        return Reg(next_reg_++);
    }

    Label fresh_label() {
        return Label(next_label_++);
    }

    void push_scope() {
        locals_.emplace_back();
    }

    void pop_scope() {
        if (locals_.size() > 1) {
            locals_.pop_back();
        }
    }

    void declare(std::string name, Reg reg, Type ty) {
        if (!locals_.empty()) {
            locals_.back().emplace(std::move(name), std::make_pair(reg, ty));
        }
    }

    std::optional<std::pair<Reg, Type>> lookup(const std::string& name) const {
        for (auto it = locals_.rbegin(); it != locals_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

    void emit(Instr instr) {
        instrs_.push_back(std::move(instr));
    }

    size_t intern_str(const std::string& s) {
        auto it = std::find(strings_.begin(), strings_.end(), s);
        if (it != strings_.end()) {
            return std::distance(strings_.begin(), it);
        }
        strings_.push_back(s);
        return strings_.size() - 1;
    }

    IrFunction finish(std::string name, size_t param_count) {
        return IrFunction{std::move(name), param_count, std::move(instrs_), std::move(strings_)};
    }

    static IrFunction lower_fn(const FnDecl& func, const std::unordered_map<std::string, Type>& fn_returns) {
        Lower low(fn_returns);
        for (size_t i = 0; i < func.params.size(); ++i) {
            Reg r(static_cast<uint32_t>(i));
            low.next_reg_ = std::max(low.next_reg_, static_cast<uint32_t>(i + 1));
            low.declare(func.params[i].name, r, func.params[i].ty);
        }
        low.lower_block(func.body, false);
        if (low.instrs_.empty() || !std::holds_alternative<InstrReturn>(low.instrs_.back().data)) {
            low.emit(Instr::Return(std::nullopt));
        }
        return low.finish(func.name, func.params.size());
    }

    static IrFunction lower_main(const std::vector<Stmt>& stmts, const std::unordered_map<std::string, Type>& fn_returns) {
        Lower low(fn_returns);
        for (const auto& stmt : stmts) {
            low.lower_stmt(stmt);
        }
        if (low.instrs_.empty() || !std::holds_alternative<InstrReturn>(low.instrs_.back().data)) {
            low.emit(Instr::Return(std::nullopt));
        }
        return low.finish("__main", 0);
    }

    void lower_block(const Block& block, bool new_scope) {
        if (new_scope) {
            push_scope();
        }
        for (const auto& stmt : block.statements) {
            lower_stmt(stmt);
        }
        if (new_scope) {
            pop_scope();
        }
    }

    void lower_stmt(const Stmt& stmt) {
        std::visit([&](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, StmtLet>) {
                auto [r, inferred] = lower_expr(val.init);
                Type t = val.ty.value_or(inferred);
                declare(val.name, r, t);
            } else if constexpr (std::is_same_v<T, StmtExpr>) {
                lower_expr(val.expr);
            } else if constexpr (std::is_same_v<T, StmtReturn>) {
                std::optional<Reg> r = std::nullopt;
                if (val.value) {
                    r = lower_expr(*val.value).first;
                }
                emit(Instr::Return(r));
            } else if constexpr (std::is_same_v<T, StmtIf>) {
                auto [c, _] = lower_expr(val.cond);
                Label else_l = fresh_label();
                Label end_l = fresh_label();
                emit(Instr::JumpIfFalse(c, else_l));
                lower_block(val.then_block, true);
                emit(Instr::Jump(end_l));
                emit(Instr::MakeLabel(else_l));
                if (val.else_block) {
                    lower_block(*val.else_block, true);
                }
                emit(Instr::MakeLabel(end_l));
            } else if constexpr (std::is_same_v<T, StmtWhile>) {
                Label head = fresh_label();
                Label end = fresh_label();
                emit(Instr::MakeLabel(head));
                auto [c, _] = lower_expr(val.cond);
                emit(Instr::JumpIfFalse(c, end));
                lower_block(val.body, true);
                emit(Instr::Jump(head));
                emit(Instr::MakeLabel(end));
            } else if constexpr (std::is_same_v<T, StmtBlock>) {
                lower_block(val.block, true);
            }
        }, stmt.node);
    }

    std::pair<Reg, Type> lower_expr(const Expr& expr) {
        return std::visit([&](const auto& val) -> std::pair<Reg, Type> {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, ExprIntLit>) {
                Reg d = fresh_reg();
                emit(Instr::ConstInt(d, val.value));
                return {d, Type::Int()};
            } else if constexpr (std::is_same_v<T, ExprBoolLit>) {
                Reg d = fresh_reg();
                emit(Instr::ConstBool(d, val.value));
                return {d, Type::Bool()};
            } else if constexpr (std::is_same_v<T, ExprStringLit>) {
                size_t idx = intern_str(val.value);
                Reg d = fresh_reg();
                emit(Instr::ConstStr(d, idx));
                return {d, Type::String()};
            } else if constexpr (std::is_same_v<T, ExprIdent>) {
                auto found = lookup(val.name);
                if (found) {
                    return *found;
                }
                return {fresh_reg(), Type::Int()};
            } else if constexpr (std::is_same_v<T, ExprUnary>) {
                auto [a, _] = lower_expr(*val.expr);
                Reg d = fresh_reg();
                if (val.op == UnaryOp::Neg) {
                    emit(Instr::Neg(d, a));
                    return {d, Type::Int()};
                } else {
                    emit(Instr::Not(d, a));
                    return {d, Type::Bool()};
                }
            } else if constexpr (std::is_same_v<T, ExprBinary>) {
                return lower_binary(val.op, *val.left, *val.right);
            } else if constexpr (std::is_same_v<T, ExprAssign>) {
                auto [v, ty] = lower_expr(*val.value);
                auto found = lookup(val.name);
                if (found) {
                    Reg dest = found->first;
                    emit(Instr::Mov(dest, v));
                    return {dest, ty};
                }
                return {v, ty};
            } else if constexpr (std::is_same_v<T, ExprCall>) {
                std::vector<Reg> arg_regs;
                Type first_ty = Type::Int();
                for (size_t i = 0; i < val.args.size(); ++i) {
                    auto [r, t] = lower_expr(val.args[i]);
                    if (i == 0) {
                        first_ty = t;
                    }
                    arg_regs.push_back(r);
                }
                if (val.callee == "print") {
                    PrintKind kind = PrintKind::Int;
                    if (first_ty == Type::Bool()) {
                        kind = PrintKind::Bool;
                    } else if (first_ty == Type::String()) {
                        kind = PrintKind::String;
                    }
                    if (!arg_regs.empty()) {
                        emit(Instr::Print(arg_regs[0], kind));
                    }
                    return {fresh_reg(), Type::Void()};
                } else {
                    Type ret = Type::Int();
                    auto it = fn_returns_.find(val.callee);
                    if (it != fn_returns_.end()) {
                        ret = it->second;
                    }
                    Reg d = fresh_reg();
                    std::optional<Reg> dest = (ret == Type::Void()) ? std::nullopt : std::make_optional(d);
                    emit(Instr::Call(dest, val.callee, std::move(arg_regs)));
                    return {d, ret};
                }
            }
            return {fresh_reg(), Type::Void()};
        }, expr.node);
    }

    std::pair<Reg, Type> lower_binary(BinaryOp op, const Expr& left, const Expr& right) {
        if (op == BinaryOp::And) {
            Reg d = fresh_reg();
            Label false_l = fresh_label();
            Label end_l = fresh_label();
            auto [l, _1] = lower_expr(left);
            emit(Instr::JumpIfFalse(l, false_l));
            auto [r, _2] = lower_expr(right);
            emit(Instr::Mov(d, r));
            emit(Instr::Jump(end_l));
            emit(Instr::MakeLabel(false_l));
            emit(Instr::ConstBool(d, false));
            emit(Instr::MakeLabel(end_l));
            return {d, Type::Bool()};
        }
        if (op == BinaryOp::Or) {
            Reg d = fresh_reg();
            Label false_l = fresh_label();
            Label end_l = fresh_label();
            auto [l, _1] = lower_expr(left);
            emit(Instr::JumpIfFalse(l, false_l));
            emit(Instr::ConstBool(d, true));
            emit(Instr::Jump(end_l));
            emit(Instr::MakeLabel(false_l));
            auto [r, _2] = lower_expr(right);
            emit(Instr::Mov(d, r));
            emit(Instr::MakeLabel(end_l));
            return {d, Type::Bool()};
        }

        auto [a, _1] = lower_expr(left);
        auto [b, _2] = lower_expr(right);
        Reg d = fresh_reg();
        Type ty = Type::Int();

        switch (op) {
            case BinaryOp::Add:
                emit(Instr::Add(d, a, b));
                ty = Type::Int();
                break;
            case BinaryOp::Sub:
                emit(Instr::Sub(d, a, b));
                ty = Type::Int();
                break;
            case BinaryOp::Mul:
                emit(Instr::Mul(d, a, b));
                ty = Type::Int();
                break;
            case BinaryOp::Div:
                emit(Instr::Div(d, a, b));
                ty = Type::Int();
                break;
            case BinaryOp::Rem:
                emit(Instr::Mod(d, a, b));
                ty = Type::Int();
                break;
            case BinaryOp::Eq:
                emit(Instr::Cmp(CmpOp::Eq, d, a, b));
                ty = Type::Bool();
                break;
            case BinaryOp::Ne:
                emit(Instr::Cmp(CmpOp::Ne, d, a, b));
                ty = Type::Bool();
                break;
            case BinaryOp::Lt:
                emit(Instr::Cmp(CmpOp::Lt, d, a, b));
                ty = Type::Bool();
                break;
            case BinaryOp::Le:
                emit(Instr::Cmp(CmpOp::Le, d, a, b));
                ty = Type::Bool();
                break;
            case BinaryOp::Gt:
                emit(Instr::Cmp(CmpOp::Gt, d, a, b));
                ty = Type::Bool();
                break;
            case BinaryOp::Ge:
                emit(Instr::Cmp(CmpOp::Ge, d, a, b));
                ty = Type::Bool();
                break;
            default:
                ty = Type::Bool();
                break;
        }
        return {d, ty};
    }

private:
    std::vector<Instr> instrs_;
    std::vector<std::string> strings_;
    uint32_t next_reg_{0};
    uint32_t next_label_{0};
    std::vector<std::unordered_map<std::string, std::pair<Reg, Type>>> locals_;
    std::unordered_map<std::string, Type> fn_returns_;
};

} // namespace

Module lower(const Program& program) {
    std::unordered_map<std::string, Type> fn_returns;
    for (const auto& f : program.functions) {
        fn_returns.emplace(f.name, f.return_type);
    }
    std::vector<IrFunction> functions;
    for (const auto& f : program.functions) {
        functions.push_back(Lower::lower_fn(f, fn_returns));
    }
    IrFunction main_fn = Lower::lower_main(program.statements, fn_returns);
    return Module{std::move(functions), std::move(main_fn)};
}

} // namespace tiny
