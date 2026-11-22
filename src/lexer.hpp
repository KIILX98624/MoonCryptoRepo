#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace moon {

enum class TokenKind { Identifier, Integer, Float, String, Character, Symbol, EndOfFile, Invalid };

struct Token {
    TokenKind kind;
    std::string text;
    std::size_t line;
    std::size_t column;
};

std::vector<Token> lex_source(const std::string& source);
const char* token_kind_name(TokenKind kind);

} // namespace moon
