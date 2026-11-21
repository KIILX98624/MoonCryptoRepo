#pragma once

#include "lexer.hpp"
#include <string>
#include <vector>

namespace moon {

struct ParameterNode { std::string name; std::string type; };
struct FunctionNode {
    std::string name;
    std::string return_type;
    std::vector<ParameterNode> parameters;
};
struct ProgramNode { std::vector<FunctionNode> functions; };

struct AstResult {
    ProgramNode program;
    std::string error;
    std::size_t line = 1;
    std::size_t column = 1;
    bool ok() const { return error.empty(); }
};

AstResult parse_ast(const std::vector<Token>& tokens);

} // namespace moon
