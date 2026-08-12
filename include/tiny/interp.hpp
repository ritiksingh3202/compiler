#pragma once

#include "tiny/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <iostream>
#include <sstream>
#include <variant>

namespace tiny {

enum class ValueKind {
    Int,
    Bool,
    Str,
    Void
};

struct Value {
    ValueKind kind{ValueKind::Void};
    std::variant<std::monostate, int64_t, bool, std::string> data;

    Value() : kind(ValueKind::Void), data(std::monostate{}) {}
    static Value Int(int64_t n) {
        Value v;
        v.kind = ValueKind::Int;
        v.data = n;
        return v;
    }
    static Value Bool(bool b) {
        Value v;
        v.kind = ValueKind::Bool;
        v.data = b;
        return v;
    }
    static Value Str(std::string s) {
        Value v;
        v.kind = ValueKind::Str;
        v.data = std::move(s);
        return v;
    }
    static Value Void() {
        return Value();
    }

    bool as_bool() const {
        if (kind == ValueKind::Bool) {
            return std::get<bool>(data);
        }
        return false;
    }

    int64_t as_int() const {
        return std::get<int64_t>(data);
    }

    const std::string& as_str() const {
        return std::get<std::string>(data);
    }

    bool operator==(const Value& other) const {
        if (kind != other.kind) return false;
        switch (kind) {
            case ValueKind::Int: return as_int() == other.as_int();
            case ValueKind::Bool: return as_bool() == other.as_bool();
            case ValueKind::Str: return as_str() == other.as_str();
            case ValueKind::Void: return true;
        }
        return false;
    }

    std::string to_string() const {
        switch (kind) {
            case ValueKind::Int: return std::to_string(as_int());
            case ValueKind::Bool: return as_bool() ? "true" : "false";
            case ValueKind::Str: return as_str();
            case ValueKind::Void: return "";
        }
        return "";
    }
};

struct RuntimeError {
    std::string message;
    Span span;
    std::string formatted;

    RuntimeError(std::string msg, Span s, std::string_view source)
        : message(std::move(msg)), span(s), formatted(format_diagnostic(message, span, source)) {}
};

std::variant<std::monostate, RuntimeError> run(const Program& program, std::string_view source, std::ostream& out);
std::variant<std::string, RuntimeError> run_to_string(const Program& program, std::string_view source);

} // namespace tiny
