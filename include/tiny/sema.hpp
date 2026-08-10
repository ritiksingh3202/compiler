#pragma once

#include "tiny/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <optional>
#include <variant>

namespace tiny {

struct SemaError {
    std::string message;
    Span span;
    std::string formatted;

    SemaError(std::string msg, Span s, std::string_view source)
        : message(std::move(msg)), span(s), formatted(format_diagnostic(message, span, source)) {}

    bool operator==(const SemaError& other) const = default;
};

struct SemaErrors {
    std::vector<SemaError> errors;

    std::string to_string() const {
        std::string res;
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i > 0) res += "\n\n";
            res += errors[i].formatted;
        }
        return res;
    }

    bool empty() const { return errors.empty(); }
};

struct FnSig {
    std::string name;
    std::vector<std::pair<std::string, Type>> params;
    Type return_type{TypeKind::Void};
    Span span;

    static FnSig from_params(std::string name, const std::vector<Param>& params, Type return_type, Span span) {
        std::vector<std::pair<std::string, Type>> ps;
        for (const auto& p : params) {
            ps.emplace_back(p.name, p.ty);
        }
        return FnSig{std::move(name), std::move(ps), return_type, span};
    }
};

class ScopeStack {
public:
    ScopeStack() {
        scopes_.emplace_back();
    }

    void push() {
        scopes_.emplace_back();
    }

    void pop() {
        if (scopes_.size() > 1) {
            scopes_.pop_back();
        }
    }

    std::optional<Type> declare(std::string name, Type ty) {
        if (scopes_.empty()) return std::nullopt;
        auto& current = scopes_.back();
        auto it = current.find(name);
        if (it != current.end()) {
            Type prev = it->second;
            it->second = ty;
            return prev;
        }
        current.emplace(std::move(name), ty);
        return std::nullopt;
    }

    const Type* lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unordered_map<std::string, Type>> scopes_;
};

std::optional<SemaErrors> check(const Program& program, std::string_view source);

} // namespace tiny
