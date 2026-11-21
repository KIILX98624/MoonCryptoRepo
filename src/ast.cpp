#include "ast.hpp"

#include <unordered_set>

namespace moon {
namespace {

struct Parser {
    const std::vector<Token>& tokens;
    std::size_t index = 0;
    AstResult result;
    std::unordered_set<std::string> function_names;

    const Token& current() const { return tokens[index]; }
    bool is(TokenKind kind, const char* text = nullptr) const {
        return current().kind == kind && (!text || current().text == text);
    }
    bool take(TokenKind kind, const char* text = nullptr) {
        if (!is(kind, text)) return false;
        ++index;
        return true;
    }
    bool fail(const char* message) {
        result.error = message;
        result.line = current().line;
        result.column = current().column;
        return false;
    }
    bool parse_function() {
        if (!take(TokenKind::Identifier, "fn")) return fail("expected fn");
        if (!is(TokenKind::Identifier)) return fail("expected function name");
        FunctionNode function;
        function.name = current().text;
        ++index;
        if (!function_names.insert(function.name).second) return fail("duplicate function definition");
        if (!take(TokenKind::Symbol, "[")) return fail("expected '[' after function name");
        if (!is(TokenKind::Symbol, "]")) {
            while (true) {
                if (!is(TokenKind::Identifier)) return fail("expected parameter name");
                ParameterNode parameter;
                parameter.name = current().text;
                ++index;
                if (!take(TokenKind::Symbol, ":")) return fail("expected ':' after parameter name");
                if (!is(TokenKind::Identifier)) return fail("expected parameter type");
                parameter.type = current().text;
                ++index;
                function.parameters.push_back(std::move(parameter));
                if (take(TokenKind::Symbol, "]")) break;
                if (!take(TokenKind::Symbol, ",")) return fail("expected ',' between parameters");
            }
        } else {
            ++index;
        }
        if (!take(TokenKind::Symbol, "->")) return fail("expected return type");
        if (!is(TokenKind::Identifier)) return fail("expected return type name");
        function.return_type = current().text;
        ++index;
        if (!take(TokenKind::Symbol, "{")) return fail("expected function body");
        int depth = 1;
        while (depth > 0 && !is(TokenKind::EndOfFile)) {
            if (take(TokenKind::Symbol, "{")) ++depth;
            else if (take(TokenKind::Symbol, "}")) --depth;
            else ++index;
        }
        if (depth != 0) return fail("unterminated function body");
        result.program.functions.push_back(std::move(function));
        return true;
    }
    AstResult run() {
        while (!is(TokenKind::EndOfFile)) {
            if (is(TokenKind::Identifier, "fn")) { if (!parse_function()) return result; }
            else ++index;
        }
        return result;
    }
};

} // namespace

AstResult parse_ast(const std::vector<Token>& tokens) {
    Parser parser{tokens, 0, {}, {}};
    return parser.run();
}

} // namespace moon
