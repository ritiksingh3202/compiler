#include "tiny/ir.hpp"
#include <unordered_map>
#include <unordered_set>

namespace tiny {

namespace {

std::optional<Reg> instr_dest(const Instr& instr) {
    return std::visit([](const auto& val) -> std::optional<Reg> {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, InstrConstInt>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrConstBool>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrConstStr>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrMov>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrAdd>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrSub>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrMul>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrDiv>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrMod>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrNeg>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrNot>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrCmp>) return val.dest;
        else if constexpr (std::is_same_v<T, InstrCall>) return val.dest;
        else return std::nullopt;
    }, instr.data);
}

void fold_constants(IrFunction& func) {
    std::unordered_map<Reg, int64_t> consts;
    std::vector<Instr> out;
    out.reserve(func.instructions.size());

    for (auto& instr : func.instructions) {
        std::optional<Instr> replacement = std::nullopt;

        if (std::holds_alternative<InstrConstInt>(instr.data)) {
            const auto& ci = std::get<InstrConstInt>(instr.data);
            consts[ci.dest] = ci.val;
        } else if (std::holds_alternative<InstrConstBool>(instr.data)) {
            const auto& cb = std::get<InstrConstBool>(instr.data);
            consts[cb.dest] = cb.val ? 1 : 0;
        } else if (std::holds_alternative<InstrMov>(instr.data)) {
            const auto& mov = std::get<InstrMov>(instr.data);
            auto it = consts.find(mov.src);
            if (it != consts.end()) {
                int64_t v = it->second;
                consts[mov.dest] = v;
                replacement = Instr::ConstInt(mov.dest, v);
            } else {
                consts.erase(mov.dest);
            }
        } else if (std::holds_alternative<InstrAdd>(instr.data)) {
            const auto& add = std::get<InstrAdd>(instr.data);
            auto ita = consts.find(add.left);
            auto itb = consts.find(add.right);
            if (ita != consts.end() && itb != consts.end()) {
                int64_t v = static_cast<int64_t>(static_cast<uint64_t>(ita->second) + static_cast<uint64_t>(itb->second));
                consts[add.dest] = v;
                replacement = Instr::ConstInt(add.dest, v);
            } else {
                consts.erase(add.dest);
            }
        } else if (std::holds_alternative<InstrSub>(instr.data)) {
            const auto& sub = std::get<InstrSub>(instr.data);
            auto ita = consts.find(sub.left);
            auto itb = consts.find(sub.right);
            if (ita != consts.end() && itb != consts.end()) {
                int64_t v = static_cast<int64_t>(static_cast<uint64_t>(ita->second) - static_cast<uint64_t>(itb->second));
                consts[sub.dest] = v;
                replacement = Instr::ConstInt(sub.dest, v);
            } else {
                consts.erase(sub.dest);
            }
        } else if (std::holds_alternative<InstrMul>(instr.data)) {
            const auto& mul = std::get<InstrMul>(instr.data);
            auto ita = consts.find(mul.left);
            auto itb = consts.find(mul.right);
            if (ita != consts.end() && itb != consts.end()) {
                int64_t v = static_cast<int64_t>(static_cast<uint64_t>(ita->second) * static_cast<uint64_t>(itb->second));
                consts[mul.dest] = v;
                replacement = Instr::ConstInt(mul.dest, v);
            } else {
                consts.erase(mul.dest);
            }
        } else if (std::holds_alternative<InstrNeg>(instr.data)) {
            const auto& neg = std::get<InstrNeg>(instr.data);
            auto it = consts.find(neg.src);
            if (it != consts.end()) {
                int64_t v = static_cast<int64_t>(0ULL - static_cast<uint64_t>(it->second));
                consts[neg.dest] = v;
                replacement = Instr::ConstInt(neg.dest, v);
            } else {
                consts.erase(neg.dest);
            }
        } else if (std::holds_alternative<InstrLabel>(instr.data) ||
                   std::holds_alternative<InstrJump>(instr.data) ||
                   std::holds_alternative<InstrJumpIfFalse>(instr.data)) {
            consts.clear();
        } else {
            auto d = instr_dest(instr);
            if (d.has_value()) {
                consts.erase(*d);
            }
        }

        out.push_back(replacement.has_value() ? std::move(*replacement) : std::move(instr));
    }

    func.instructions = std::move(out);
}

void mark_uses(const Instr& instr, std::unordered_set<Reg>& used) {
    std::visit([&](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, InstrReturn>) {
            if (val.val.has_value()) used.insert(*val.val);
        } else if constexpr (std::is_same_v<T, InstrPrint>) {
            used.insert(val.reg);
        } else if constexpr (std::is_same_v<T, InstrJumpIfFalse>) {
            used.insert(val.cond);
        } else if constexpr (std::is_same_v<T, InstrCall>) {
            for (const auto& a : val.args) {
                used.insert(a);
            }
        } else if constexpr (std::is_same_v<T, InstrMov>) {
            if (used.count(val.dest)) {
                used.insert(val.src);
            }
        } else if constexpr (std::is_same_v<T, InstrAdd>) {
            if (used.count(val.dest)) {
                used.insert(val.left);
                used.insert(val.right);
            }
        } else if constexpr (std::is_same_v<T, InstrSub>) {
            if (used.count(val.dest)) {
                used.insert(val.left);
                used.insert(val.right);
            }
        } else if constexpr (std::is_same_v<T, InstrMul>) {
            if (used.count(val.dest)) {
                used.insert(val.left);
                used.insert(val.right);
            }
        } else if constexpr (std::is_same_v<T, InstrDiv>) {
            if (used.count(val.dest)) {
                used.insert(val.left);
                used.insert(val.right);
            }
        } else if constexpr (std::is_same_v<T, InstrMod>) {
            if (used.count(val.dest)) {
                used.insert(val.left);
                used.insert(val.right);
            }
        } else if constexpr (std::is_same_v<T, InstrCmp>) {
            if (used.count(val.dest)) {
                used.insert(val.left);
                used.insert(val.right);
            }
        } else if constexpr (std::is_same_v<T, InstrNeg>) {
            if (used.count(val.dest)) {
                used.insert(val.src);
            }
        } else if constexpr (std::is_same_v<T, InstrNot>) {
            if (used.count(val.dest)) {
                used.insert(val.src);
            }
        }
    }, instr.data);
}

bool keep_instr(const Instr& instr, const std::unordered_set<Reg>& used) {
    return std::visit([&](const auto& val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, InstrJump> ||
                      std::is_same_v<T, InstrJumpIfFalse> ||
                      std::is_same_v<T, InstrLabel> ||
                      std::is_same_v<T, InstrReturn> ||
                      std::is_same_v<T, InstrPrint> ||
                      std::is_same_v<T, InstrCall>) {
            return true;
        } else {
            auto d = instr_dest(instr);
            return d.has_value() && used.count(*d) > 0;
        }
    }, instr.data);
}

void eliminate_dead(IrFunction& func) {
    // Drop unreachable after return
    std::vector<Instr> out;
    bool dead = false;
    for (auto& instr : func.instructions) {
        if (std::holds_alternative<InstrLabel>(instr.data)) {
            dead = false;
        }
        if (dead) {
            continue;
        }
        if (std::holds_alternative<InstrReturn>(instr.data)) {
            out.push_back(std::move(instr));
            dead = true;
            continue;
        }
        out.push_back(std::move(instr));
    }

    // Collect used registers iteratively backward
    std::unordered_set<Reg> used;
    for (auto it = out.rbegin(); it != out.rend(); ++it) {
        mark_uses(*it, used);
    }

    std::vector<Instr> filtered;
    filtered.reserve(out.size());
    for (auto& instr : out) {
        if (keep_instr(instr, used)) {
            filtered.push_back(std::move(instr));
        }
    }
    func.instructions = std::move(filtered);
}

void optimize_function(IrFunction& func) {
    fold_constants(func);
    eliminate_dead(func);
}

} // namespace

void optimize(Module& module) {
    for (auto& f : module.functions) {
        optimize_function(f);
    }
    optimize_function(module.main);
}

} // namespace tiny
