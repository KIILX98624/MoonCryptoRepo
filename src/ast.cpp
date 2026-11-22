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

    bool parse_c_function(ExternalNode& external) {
        if (!is(TokenKind::Identifier)) return fail("expected C return type in playCodeC block");
        CFunctionNode function;
        function.return_type = current().text;
        ++index;
        // Preserve pointer return declarators such as void* malloc(...).
        while (take(TokenKind::Symbol, "*")) function.return_type += "*";

        if (!is(TokenKind::Identifier)) return fail("expected C function name");
        function.name = current().text;
        ++index;
        if (!take(TokenKind::Symbol, "(")) return fail("expected '(' after C function name");

        if (is(TokenKind::Identifier, "void") &&
            index + 1 < tokens.size() &&
            tokens[index + 1].kind == TokenKind::Symbol &&
            tokens[index + 1].text == ")") {
            // In C, a lone void means that the function takes no parameters.
            ++index;
            ++index;
        } else if (!is(TokenKind::Symbol, ")")) {
            while (true) {
                CParameterNode parameter;
                if (is(TokenKind::Identifier, "const")) {
                    parameter.type = "const ";
                    ++index;
                }

                if (!is(TokenKind::Identifier)) return fail("expected C parameter type");
                const std::string first_type_word = current().text;
                parameter.type += first_type_word;
                ++index;

                // C type specifiers can contain multiple words, e.g. "unsigned int".
                if ((first_type_word == "unsigned" || first_type_word == "signed") &&
                    is(TokenKind::Identifier)) {
                    parameter.type += " " + current().text;
                    ++index;
                } else if (first_type_word == "long" && is(TokenKind::Identifier, "long")) {
                    parameter.type += " " + current().text;
                    ++index;
                    if (is(TokenKind::Identifier, "int")) {
                        parameter.type += " " + current().text;
                        ++index;
                    }
                } else if (first_type_word == "short" && is(TokenKind::Identifier, "int")) {
                    parameter.type += " " + current().text;
                    ++index;
                }

                // Preserve pointer declarators (char*, const char*, char**, ...).
                while (take(TokenKind::Symbol, "*")) {
                    parameter.type += "*";
                }

                if (!is(TokenKind::Identifier)) return fail("expected C parameter name");
                parameter.name = current().text;
                ++index;
                function.parameters.push_back(std::move(parameter));

                if (take(TokenKind::Symbol, ")")) break;
                if (!take(TokenKind::Symbol, ",")) return fail("expected ',' between C parameters");
            }
        } else {
            ++index;
        }

        take(TokenKind::Symbol, ";");
        external.c_functions.push_back(std::move(function));
        return true;
    }

    bool parse_external() {
        if (!take(TokenKind::Symbol, "#") ||
            !take(TokenKind::Identifier, "external")) return fail("expected #external");
        if (!is(TokenKind::String)) return fail("expected header path after #external");

        ExternalNode external;
        external.path = current().text;
        if (external.path.size() >= 2 && external.path.front() == '"' && external.path.back() == '"') {
            external.path = external.path.substr(1, external.path.size() - 2);
        }
        ++index;

        if (take(TokenKind::Symbol, ";")) {
            result.program.externals.push_back(std::move(external));
            return true;
        }

        if (!take(TokenKind::Symbol, "{")) {
            result.program.externals.push_back(std::move(external));
            return true;
        }

        bool found_play_code = false;
        while (!is(TokenKind::EndOfFile) && !is(TokenKind::Symbol, "}")) {
            if (is(TokenKind::Identifier, "playCodeC")) {
                ++index;
                if (!take(TokenKind::Symbol, "{")) return fail("expected '{' after playCodeC");
                found_play_code = true;
                while (!is(TokenKind::EndOfFile) && !is(TokenKind::Symbol, "}")) {
                    if (!parse_c_function(external)) return false;
                }
                if (!take(TokenKind::Symbol, "}")) return fail("unterminated playCodeC block");
            } else {
                return fail("expected playCodeC block inside #external");
            }
        }

        if (!take(TokenKind::Symbol, "}")) return fail("unterminated #external block");
        if (!found_play_code) return fail("#external block must contain playCodeC");
        result.program.externals.push_back(std::move(external));
        return true;
    }

    AstResult run() {
        while (!is(TokenKind::EndOfFile)) {
            if (is(TokenKind::Identifier, "fn")) {
                if (!parse_function()) return result;
            } else if (is(TokenKind::Identifier, "const")) {
                ++index;
                if (!is(TokenKind::Identifier)) {
                    fail("expected constant identifier");
                    return result;
                }
                std::string cname = current().text;
                ++index;
                std::string ctype = "Inferred";
                if (take(TokenKind::Symbol, ":")) {
                    if (!is(TokenKind::Identifier)) {
                        fail("expected constant type");
                        return result;
                    }
                    ctype = current().text;
                    ++index;
                }
                result.program.constants.push_back({std::move(cname), std::move(ctype)});
                while (!is(TokenKind::EndOfFile) && !take(TokenKind::Symbol, ";")) ++index;
            } else if (is(TokenKind::Identifier, "struct") || is(TokenKind::Identifier, "class")) {
                bool is_cls = current().text == "class";
                ++index;
                if (!is(TokenKind::Identifier)) {
                    fail("expected type identifier");
                    return result;
                }
                std::string sname = current().text;
                ++index;
                result.program.types.push_back({std::move(sname), is_cls});
                if (take(TokenKind::Symbol, "{")) {
                    int depth = 1;
                    while (depth > 0 && !is(TokenKind::EndOfFile)) {
                        if (take(TokenKind::Symbol, "{")) ++depth;
                        else if (take(TokenKind::Symbol, "}")) --depth;
                        else ++index;
                    }
                }
            } else if (is(TokenKind::Symbol, "#")) {
                if (index + 1 < tokens.size() && tokens[index + 1].kind == TokenKind::Identifier &&
                    tokens[index + 1].text == "external") {
                    if (!parse_external()) return result;
                } else {
                    ++index;
                }
            } else {
                ++index;
            }
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
