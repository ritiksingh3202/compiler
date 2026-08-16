#pragma once

#include "tiny/ast.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace tiny {

struct Reg {
    uint32_t id{0};
    constexpr Reg() = default;
    constexpr explicit Reg(uint32_t i) : id(i) {}
    bool operator==(const Reg& other) const = default;
    bool operator!=(const Reg& other) const = default;
    std::string to_string() const { return "r" + std::to_string(id); }
};

struct Label {
    uint32_t id{0};
    constexpr Label() = default;
    constexpr explicit Label(uint32_t i) : id(i) {}
    bool operator==(const Label& other) const = default;
    bool operator!=(const Label& other) const = default;
    std::string to_string() const { return "L" + std::to_string(id); }
};

enum class CmpOp {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge
};

enum class PrintKind {
    Int,
    Bool,
    String
};

struct InstrConstInt { Reg dest; int64_t val; };
struct InstrConstBool { Reg dest; bool val; };
struct InstrConstStr { Reg dest; size_t pool_idx; };
struct InstrMov { Reg dest; Reg src; };
struct InstrAdd { Reg dest; Reg left; Reg right; };
struct InstrSub { Reg dest; Reg left; Reg right; };
struct InstrMul { Reg dest; Reg left; Reg right; };
struct InstrDiv { Reg dest; Reg left; Reg right; };
struct InstrMod { Reg dest; Reg left; Reg right; };
struct InstrNeg { Reg dest; Reg src; };
struct InstrNot { Reg dest; Reg src; };
struct InstrCmp { CmpOp op; Reg dest; Reg left; Reg right; };
struct InstrJump { Label target; };
struct InstrJumpIfFalse { Reg cond; Label target; };
struct InstrLabel { Label label; };
struct InstrCall { std::optional<Reg> dest; std::string name; std::vector<Reg> args; };
struct InstrReturn { std::optional<Reg> val; };
struct InstrPrint { Reg reg; PrintKind kind; };

using InstrData = std::variant<
    InstrConstInt,
    InstrConstBool,
    InstrConstStr,
    InstrMov,
    InstrAdd,
    InstrSub,
    InstrMul,
    InstrDiv,
    InstrMod,
    InstrNeg,
    InstrNot,
    InstrCmp,
    InstrJump,
    InstrJumpIfFalse,
    InstrLabel,
    InstrCall,
    InstrReturn,
    InstrPrint
>;

struct Instr {
    InstrData data;

    Instr(InstrData d) : data(std::move(d)) {}

    static Instr ConstInt(Reg d, int64_t v) { return Instr(InstrConstInt{d, v}); }
    static Instr ConstBool(Reg d, bool v) { return Instr(InstrConstBool{d, v}); }
    static Instr ConstStr(Reg d, size_t i) { return Instr(InstrConstStr{d, i}); }
    static Instr Mov(Reg d, Reg s) { return Instr(InstrMov{d, s}); }
    static Instr Add(Reg d, Reg l, Reg r) { return Instr(InstrAdd{d, l, r}); }
    static Instr Sub(Reg d, Reg l, Reg r) { return Instr(InstrSub{d, l, r}); }
    static Instr Mul(Reg d, Reg l, Reg r) { return Instr(InstrMul{d, l, r}); }
    static Instr Div(Reg d, Reg l, Reg r) { return Instr(InstrDiv{d, l, r}); }
    static Instr Mod(Reg d, Reg l, Reg r) { return Instr(InstrMod{d, l, r}); }
    static Instr Neg(Reg d, Reg s) { return Instr(InstrNeg{d, s}); }
    static Instr Not(Reg d, Reg s) { return Instr(InstrNot{d, s}); }
    static Instr Cmp(CmpOp op, Reg d, Reg l, Reg r) { return Instr(InstrCmp{op, d, l, r}); }
    static Instr Jump(Label l) { return Instr(InstrJump{l}); }
    static Instr JumpIfFalse(Reg r, Label l) { return Instr(InstrJumpIfFalse{r, l}); }
    static Instr MakeLabel(Label l) { return Instr(InstrLabel{l}); }
    static Instr Call(std::optional<Reg> dest, std::string name, std::vector<Reg> args) {
        return Instr(InstrCall{dest, std::move(name), std::move(args)});
    }
    static Instr Return(std::optional<Reg> val) { return Instr(InstrReturn{val}); }
    static Instr Print(Reg r, PrintKind kind) { return Instr(InstrPrint{r, kind}); }

    bool operator==(const Instr& other) const = default;

    std::string to_string() const;
};

struct IrFunction {
    std::string name;
    size_t param_count{0};
    std::vector<Instr> instructions;
    std::vector<std::string> string_pool;

    std::string display() const;
};

struct Module {
    std::vector<IrFunction> functions;
    IrFunction main;

    std::string display() const;
};

Module lower(const Program& program);
void optimize(Module& module);

} // namespace tiny

template <>
struct std::hash<tiny::Reg> {
    size_t operator()(const tiny::Reg& r) const noexcept {
        return std::hash<uint32_t>{}(r.id);
    }
};

template <>
struct std::hash<tiny::Label> {
    size_t operator()(const tiny::Label& l) const noexcept {
        return std::hash<uint32_t>{}(l.id);
    }
};
