#include "lexer.hpp"

#include <cctype>

namespace moon {

const char* token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::Identifier: return "identifier";
        case TokenKind::Integer: return "integer";
        case TokenKind::String: return "string";
        case TokenKind::Character: return "character";
        case TokenKind::Symbol: return "symbol";
        case TokenKind::EndOfFile: return "eof";
        case TokenKind::Invalid: return "invalid";
    }
    return "invalid";
}

std::vector<Token> lex_source(const std::string& source) {
    std::vector<Token> tokens;
    std::size_t i = 0, line = 1, column = 1;
    auto advance = [&]() {
        if (source[i] == '\n') { ++line; column = 1; }
        else { ++column; }
        ++i;
    };
    while (i < source.size()) {
        const char c = source[i];
        if (std::isspace(static_cast<unsigned char>(c))) { advance(); continue; }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n') advance();
            continue;
        }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            advance(); advance();
            while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) advance();
            if (i + 1 < source.size()) { advance(); advance(); }
            continue;
        }
        const auto start_line = line;
        const auto start_column = column;
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const auto start = i;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) advance();
            tokens.push_back({TokenKind::Identifier, source.substr(start, i - start), start_line, start_column});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            const auto start = i;
            while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) advance();
            tokens.push_back({TokenKind::Integer, source.substr(start, i - start), start_line, start_column});
            continue;
        }
        if (c == '"' || c == '\'') {
            const char quote = c;
            const auto start = i++;
            ++column;
            bool closed = false;
            while (i < source.size()) {
                if (source[i] == '\\' && i + 1 < source.size()) { advance(); advance(); continue; }
                if (source[i] == quote) { advance(); closed = true; break; }
                advance();
            }
            tokens.push_back({closed ? (quote == '"' ? TokenKind::String : TokenKind::Character) : TokenKind::Invalid,
                              source.substr(start, i - start), start_line, start_column});
            continue;
        }
        const char* double_symbols[] = {"->", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>"};
        bool matched = false;
        for (const char* symbol : double_symbols) {
            if (source.compare(i, 2, symbol) == 0) {
                tokens.push_back({TokenKind::Symbol, symbol, start_line, start_column});
                advance(); advance(); matched = true; break;
            }
        }
        if (matched) continue;
        const std::string symbols = "{}[]();,:=+-*/%!<>|&^~.#";
        if (symbols.find(c) != std::string::npos) {
            tokens.push_back({TokenKind::Symbol, std::string(1, c), start_line, start_column});
            advance();
        } else {
            tokens.push_back({TokenKind::Invalid, std::string(1, c), start_line, start_column});
            advance();
        }
    }
    tokens.push_back({TokenKind::EndOfFile, "", line, column});
    return tokens;
}

} // namespace moon
