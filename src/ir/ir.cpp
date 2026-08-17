#include "tiny/ir.hpp"
#include <sstream>

namespace tiny {

std::string Instr::to_string() const {
    return std::visit([&](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, InstrConstInt>) {
            return val.dest.to_string() + " = " + std::to_string(val.val);
        } else if constexpr (std::is_same_v<T, InstrConstBool>) {
            return val.dest.to_string() + " = " + (val.val ? "true" : "false");
        } else if constexpr (std::is_same_v<T, InstrConstStr>) {
            return val.dest.to_string() + " = str[" + std::to_string(val.pool_idx) + "]";
        } else if constexpr (std::is_same_v<T, InstrMov>) {
            return val.dest.to_string() + " = " + val.src.to_string();
        } else if constexpr (std::is_same_v<T, InstrAdd>) {
            return val.dest.to_string() + " = " + val.left.to_string() + " + " + val.right.to_string();
        } else if constexpr (std::is_same_v<T, InstrSub>) {
            return val.dest.to_string() + " = " + val.left.to_string() + " - " + val.right.to_string();
        } else if constexpr (std::is_same_v<T, InstrMul>) {
            return val.dest.to_string() + " = " + val.left.to_string() + " * " + val.right.to_string();
        } else if constexpr (std::is_same_v<T, InstrDiv>) {
            return val.dest.to_string() + " = " + val.left.to_string() + " / " + val.right.to_string();
        } else if constexpr (std::is_same_v<T, InstrMod>) {
            return val.dest.to_string() + " = " + val.left.to_string() + " % " + val.right.to_string();
        } else if constexpr (std::is_same_v<T, InstrNeg>) {
            return val.dest.to_string() + " = -" + val.src.to_string();
        } else if constexpr (std::is_same_v<T, InstrNot>) {
            return val.dest.to_string() + " = !" + val.src.to_string();
        } else if constexpr (std::is_same_v<T, InstrCmp>) {
            std::string sym;
            switch (val.op) {
                case CmpOp::Eq: sym = "=="; break;
                case CmpOp::Ne: sym = "!="; break;
                case CmpOp::Lt: sym = "<"; break;
                case CmpOp::Le: sym = "<="; break;
                case CmpOp::Gt: sym = ">"; break;
                case CmpOp::Ge: sym = ">="; break;
            }
            return val.dest.to_string() + " = " + val.left.to_string() + " " + sym + " " + val.right.to_string();
        } else if constexpr (std::is_same_v<T, InstrJump>) {
            return "goto " + val.target.to_string();
        } else if constexpr (std::is_same_v<T, InstrJumpIfFalse>) {
            return "if !" + val.cond.to_string() + " goto " + val.target.to_string();
        } else if constexpr (std::is_same_v<T, InstrLabel>) {
            return val.label.to_string() + ":";
        } else if constexpr (std::is_same_v<T, InstrCall>) {
            std::string args_s;
            for (size_t i = 0; i < val.args.size(); ++i) {
                if (i > 0) args_s += ", ";
                args_s += val.args[i].to_string();
            }
            if (val.dest.has_value()) {
                return val.dest->to_string() + " = call " + val.name + "(" + args_s + ")";
            } else {
                return "call " + val.name + "(" + args_s + ")";
            }
        } else if constexpr (std::is_same_v<T, InstrReturn>) {
            if (val.val.has_value()) {
                return "return " + val.val->to_string();
            } else {
                return "return";
            }
        } else if constexpr (std::is_same_v<T, InstrPrint>) {
            switch (val.kind) {
                case PrintKind::Int: return "print_int " + val.reg.to_string();
                case PrintKind::Bool: return "print_bool " + val.reg.to_string();
                case PrintKind::String: return "print_str " + val.reg.to_string();
            }
        }
        return "";
    }, data);
}

std::string IrFunction::display() const {
    std::string out = "fn " + name + " (params=" + std::to_string(param_count) + "):\n";
    for (size_t i = 0; i < string_pool.size(); ++i) {
        std::string escaped = "\"";
        for (char c : string_pool[i]) {
            if (c == '\\') escaped += "\\\\";
            else if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\t') escaped += "\\t";
            else if (c == '\r') escaped += "\\r";
            else escaped += c;
        }
        escaped += "\"";
        out += "  str[" + std::to_string(i) + "] = " + escaped + "\n";
    }
    for (const auto& instr : instructions) {
        if (std::holds_alternative<InstrLabel>(instr.data)) {
            out += instr.to_string() + "\n";
        } else {
            out += "  " + instr.to_string() + "\n";
        }
    }
    return out;
}

std::string Module::display() const {
    std::string out;
    for (const auto& f : functions) {
        out += f.display();
        out += "\n";
    }
    out += main.display();
    return out;
}

} // namespace tiny
