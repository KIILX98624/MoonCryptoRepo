#pragma once

#include "lexer.hpp"
#include <string>
#include <vector>

namespace moon {

struct ParameterNode {
    std::string name;
    std::string type;
};

struct FunctionNode {
    std::string name;
    std::string return_type;
    std::vector<ParameterNode> parameters;
};

struct ConstantNode {
    std::string name;
    std::string type;
};

struct StructNode {
    std::string name;
    bool is_class = false;
};

struct ExternalNode {
    std::string path;
};

struct ProgramNode {
    std::vector<FunctionNode> functions;
    std::vector<ConstantNode> constants;
    std::vector<StructNode> types;
    std::vector<ExternalNode> externals;
};

struct AstResult {
    ProgramNode program;
    std::string error;
    std::size_t line = 1;
    std::size_t column = 1;
    bool ok() const { return error.empty(); }
};

AstResult parse_ast(const std::vector<Token>& tokens);

} // namespace moon
