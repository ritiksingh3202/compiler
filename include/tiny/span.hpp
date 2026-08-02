#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <algorithm>
#include <sstream>

namespace tiny {

/// 1-based line and column in the source text.
struct Position {
    uint32_t line{1};
    uint32_t column{1};

    constexpr Position() = default;
    constexpr Position(uint32_t l, uint32_t c) : line(l), column(c) {}

    static constexpr Position start() {
        return Position(1, 1);
    }

    bool operator==(const Position& other) const = default;
    bool operator!=(const Position& other) const = default;

    std::string to_string() const {
        return std::to_string(line) + ":" + std::to_string(column);
    }
};

/// Half-open source span: [start, end).
struct Span {
    Position start;
    Position end;

    constexpr Span() = default;
    constexpr Span(Position s, Position e) : start(s), end(e) {}

    static constexpr Span empty(Position pos) {
        return Span(pos, pos);
    }

    bool operator==(const Span& other) const = default;
    bool operator!=(const Span& other) const = default;

    std::string to_string() const {
        if (start.line == end.line) {
            return std::to_string(start.line) + ":" + std::to_string(start.column) + "-" + std::to_string(end.column);
        } else {
            return start.to_string() + "-" + end.to_string();
        }
    }
};

/// Formats a rustc-style diagnostic with line numbers and caret.
inline std::string format_diagnostic(std::string_view message, Span span, std::string_view source) {
    uint32_t line_no = span.start.line;
    uint32_t col = span.start.column;

    std::string_view line_text = "";
    uint32_t current_line = 1;
    size_t line_start = 0;

    for (size_t i = 0; i <= source.size(); ++i) {
        if (i == source.size() || source[i] == '\n') {
            if (current_line == line_no) {
                size_t len = i - line_start;
                if (len > 0 && source[line_start + len - 1] == '\r') {
                    len--;
                }
                line_text = source.substr(line_start, len);
                break;
            }
            current_line++;
            line_start = i + 1;
        }
    }

    std::string gutter = std::to_string(line_no);
    std::string pad(gutter.length(), ' ');

    size_t caret_col = (col > 0) ? (col - 1) : 0;
    size_t caret_len = 1;
    if (span.end.line == span.start.line && span.end.column > span.start.column) {
        caret_len = span.end.column - span.start.column;
    }
    std::string caret = std::string(caret_col, ' ') + std::string(caret_len, '^');

    std::ostringstream oss;
    oss << "error: " << message << "\n"
        << pad << " --> " << line_no << ":" << col << "\n"
        << pad << "  |\n"
        << gutter << "  | " << line_text << "\n"
        << pad << "  | " << caret;
    return oss.str();
}

} // namespace tiny
