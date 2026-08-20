#include "tiny/codegen.hpp"
#include <algorithm>
#include <sstream>

namespace tiny {

namespace {

std::string escape_c_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    out += "\"";
    return out;
}

int32_t slot(Reg reg) {
    return -8 * (static_cast<int32_t>(reg.id) + 1);
}

std::optional<uint32_t> instr_max_reg(const Instr& instr) {
    return std::visit([](const auto& val) -> std::optional<uint32_t> {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, InstrConstInt>) return val.dest.id;
        else if constexpr (std::is_same_v<T, InstrConstBool>) return val.dest.id;
        else if constexpr (std::is_same_v<T, InstrConstStr>) return val.dest.id;
        else if constexpr (std::is_same_v<T, InstrNeg>) return val.dest.id;
        else if constexpr (std::is_same_v<T, InstrNot>) return val.dest.id;
        else if constexpr (std::is_same_v<T, InstrMov>) return std::max(val.dest.id, val.src.id);
        else if constexpr (std::is_same_v<T, InstrAdd>) return std::max({val.dest.id, val.left.id, val.right.id});
        else if constexpr (std::is_same_v<T, InstrSub>) return std::max({val.dest.id, val.left.id, val.right.id});
        else if constexpr (std::is_same_v<T, InstrMul>) return std::max({val.dest.id, val.left.id, val.right.id});
        else if constexpr (std::is_same_v<T, InstrDiv>) return std::max({val.dest.id, val.left.id, val.right.id});
        else if constexpr (std::is_same_v<T, InstrMod>) return std::max({val.dest.id, val.left.id, val.right.id});
        else if constexpr (std::is_same_v<T, InstrCmp>) return std::max({val.dest.id, val.left.id, val.right.id});
        else if constexpr (std::is_same_v<T, InstrJumpIfFalse>) return val.cond.id;
        else if constexpr (std::is_same_v<T, InstrPrint>) return val.reg.id;
        else if constexpr (std::is_same_v<T, InstrCall>) {
            uint32_t m = val.dest ? val.dest->id : 0;
            for (const auto& a : val.args) {
                m = std::max(m, a.id);
            }
            return m;
        } else if constexpr (std::is_same_v<T, InstrReturn>) {
            return val.val ? std::make_optional(val.val->id) : std::nullopt;
        } else {
            return std::nullopt;
        }
    }, instr.data);
}

std::string label_name(const std::string& func_name, bool is_main, uint32_t l) {
    if (is_main) {
        return ".Lm_" + std::to_string(l);
    } else {
        return ".L_" + func_name + "_" + std::to_string(l);
    }
}

void bin_op(std::string& out, Reg d, Reg a, Reg b, const char* op) {
    out += "  movq " + std::to_string(slot(a)) + "(%rbp), %rax\n";
    out += "  " + std::string(op) + " " + std::to_string(slot(b)) + "(%rbp), %rax\n";
    out += "  movq %rax, " + std::to_string(slot(d)) + "(%rbp)\n";
}

void emit_instr(std::string& out, const Instr& instr, size_t fi, const std::string& func_name, bool is_main) {
    std::visit([&](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, InstrConstInt>) {
            out += "  movq $" + std::to_string(val.val) + ", " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrConstBool>) {
            int n = val.val ? 1 : 0;
            out += "  movq $" + std::to_string(n) + ", " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrConstStr>) {
            out += "  leaq .str_" + std::to_string(fi) + "_" + std::to_string(val.pool_idx) + "(%rip), %rax\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrMov>) {
            out += "  movq " + std::to_string(slot(val.src)) + "(%rbp), %rax\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrAdd>) {
            bin_op(out, val.dest, val.left, val.right, "addq");
        } else if constexpr (std::is_same_v<T, InstrSub>) {
            bin_op(out, val.dest, val.left, val.right, "subq");
        } else if constexpr (std::is_same_v<T, InstrMul>) {
            out += "  movq " + std::to_string(slot(val.left)) + "(%rbp), %rax\n";
            out += "  imulq " + std::to_string(slot(val.right)) + "(%rbp), %rax\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrDiv>) {
            out += "  movq " + std::to_string(slot(val.left)) + "(%rbp), %rax\n";
            out += "  cqo\n";
            out += "  idivq " + std::to_string(slot(val.right)) + "(%rbp)\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrMod>) {
            out += "  movq " + std::to_string(slot(val.left)) + "(%rbp), %rax\n";
            out += "  cqo\n";
            out += "  idivq " + std::to_string(slot(val.right)) + "(%rbp)\n";
            out += "  movq %rdx, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrNeg>) {
            out += "  movq " + std::to_string(slot(val.src)) + "(%rbp), %rax\n";
            out += "  negq %rax\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrNot>) {
            out += "  movq " + std::to_string(slot(val.src)) + "(%rbp), %rax\n";
            out += "  xorq $1, %rax\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrCmp>) {
            out += "  movq " + std::to_string(slot(val.left)) + "(%rbp), %rax\n";
            out += "  cmpq " + std::to_string(slot(val.right)) + "(%rbp), %rax\n";
            const char* set = "";
            switch (val.op) {
                case CmpOp::Eq: set = "sete"; break;
                case CmpOp::Ne: set = "setne"; break;
                case CmpOp::Lt: set = "setl"; break;
                case CmpOp::Le: set = "setle"; break;
                case CmpOp::Gt: set = "setg"; break;
                case CmpOp::Ge: set = "setge"; break;
            }
            out += "  " + std::string(set) + " %al\n";
            out += "  movzbq %al, %rax\n";
            out += "  movq %rax, " + std::to_string(slot(val.dest)) + "(%rbp)\n";
        } else if constexpr (std::is_same_v<T, InstrLabel>) {
            out += label_name(func_name, is_main, val.label.id) + ":\n";
        } else if constexpr (std::is_same_v<T, InstrJump>) {
            out += "  jmp " + label_name(func_name, is_main, val.target.id) + "\n";
        } else if constexpr (std::is_same_v<T, InstrJumpIfFalse>) {
            out += "  cmpq $0, " + std::to_string(slot(val.cond)) + "(%rbp)\n";
            out += "  je " + label_name(func_name, is_main, val.target.id) + "\n";
        } else if constexpr (std::is_same_v<T, InstrCall>) {
            const char* arg_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
            size_t n = std::min<size_t>(val.args.size(), 6);
            for (size_t i = 0; i < n; ++i) {
                out += "  movq " + std::to_string(slot(val.args[i])) + "(%rbp), " + arg_regs[i] + "\n";
            }
            out += "  xorl %eax, %eax\n";
            out += "  call tiny_" + val.name + "\n";
            if (val.dest.has_value()) {
                out += "  movq %rax, " + std::to_string(slot(*val.dest)) + "(%rbp)\n";
            }
        } else if constexpr (std::is_same_v<T, InstrReturn>) {
            if (val.val.has_value()) {
                out += "  movq " + std::to_string(slot(*val.val)) + "(%rbp), %rax\n";
            } else {
                out += "  xorq %rax, %rax\n";
            }
            out += "  jmp .Lret_" + std::to_string(fi) + "\n";
        } else if constexpr (std::is_same_v<T, InstrPrint>) {
            switch (val.kind) {
                case PrintKind::Int:
                    out += "  movq " + std::to_string(slot(val.reg)) + "(%rbp), %rsi\n";
                    out += "  leaq .fmt_int(%rip), %rdi\n";
                    out += "  xorl %eax, %eax\n";
                    out += "  call printf\n";
                    break;
                case PrintKind::String:
                    out += "  movq " + std::to_string(slot(val.reg)) + "(%rbp), %rsi\n";
                    out += "  leaq .fmt_str(%rip), %rdi\n";
                    out += "  xorl %eax, %eax\n";
                    out += "  call printf\n";
                    break;
                case PrintKind::Bool:
                    out += "  cmpq $0, " + std::to_string(slot(val.reg)) + "(%rbp)\n";
                    out += "  leaq .false_str(%rip), %rdi\n";
                    out += "  leaq .true_str(%rip), %rax\n";
                    out += "  cmovne %rax, %rdi\n";
                    out += "  xorl %eax, %eax\n";
                    out += "  call printf\n";
                    break;
            }
        }
    }, instr.data);
}

void emit_function(std::string& out, const IrFunction& func, size_t fi, bool is_main) {
    if (is_main) {
        out += "main:\n";
    } else {
        out += ".globl tiny_" + func.name + "\n";
        out += "tiny_" + func.name + ":\n";
    }

    // Prologue
    uint32_t max_reg = 0;
    for (const auto& instr : func.instructions) {
        auto m = instr_max_reg(instr);
        if (m.has_value()) {
            max_reg = std::max(max_reg, *m);
        }
    }
    int32_t frame = ((static_cast<int32_t>(max_reg) + 1) * 8 + 15) & ~15;

    out += "  pushq %rbp\n";
    out += "  movq %rsp, %rbp\n";
    if (frame > 0) {
        out += "  subq $" + std::to_string(frame) + ", %rsp\n";
    }

    const char* arg_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    size_t n = std::min<size_t>(func.param_count, 6);
    for (size_t i = 0; i < n; ++i) {
        int32_t s = slot(Reg(static_cast<uint32_t>(i)));
        out += "  movq " + std::string(arg_regs[i]) + ", " + std::to_string(s) + "(%rbp)\n";
    }

    for (const auto& instr : func.instructions) {
        emit_instr(out, instr, fi, func.name, is_main);
    }

    // Epilogue fallback
    out += ".Lret_" + std::to_string(fi) + ":\n";
    out += "  leave\n";
    out += "  ret\n";
}

} // namespace

std::string emit_asm(const Module& module) {
    std::string out;
    out += ".text\n";
    out += ".globl main\n";

    out += ".section .rodata\n";
    out += ".fmt_int:\n";
    out += "  .string \"%ld\\n\"\n";
    out += ".fmt_str:\n";
    out += "  .string \"%s\\n\"\n";
    out += ".true_str:\n";
    out += "  .string \"true\\n\"\n";
    out += ".false_str:\n";
    out += "  .string \"false\\n\"\n";

    for (size_t fi = 0; fi < module.functions.size(); ++fi) {
        const auto& func = module.functions[fi];
        for (size_t si = 0; si < func.string_pool.size(); ++si) {
            out += ".str_" + std::to_string(fi) + "_" + std::to_string(si) + ":\n";
            out += "  .string " + escape_c_string(func.string_pool[si]) + "\n";
        }
    }
    size_t main_fi = module.functions.size();
    for (size_t si = 0; si < module.main.string_pool.size(); ++si) {
        out += ".str_" + std::to_string(main_fi) + "_" + std::to_string(si) + ":\n";
        out += "  .string " + escape_c_string(module.main.string_pool[si]) + "\n";
    }

    out += ".text\n";

    for (size_t fi = 0; fi < module.functions.size(); ++fi) {
        emit_function(out, module.functions[fi], fi, false);
    }
    emit_function(out, module.main, main_fi, true);

    return out;
}

} // namespace tiny
