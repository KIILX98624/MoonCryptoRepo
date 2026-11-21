#include "project.hpp"
#include "lexer.hpp"
#include "ast.hpp"

#include <filesystem>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(MOONCRYPTO_USE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

namespace {

void configure_console_encoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void print_usage() {
    std::cout << "MoonCrypto compiler prototype 0.1.0\n"
              << "Usage:\n"
              << "  moon entry [project-directory]\n"
              << "  moon run [project-directory]\n"
              << "  mooncyt nome_do_arquivo.mncyt\n"
              << "  mooncyt lex arquivo.mncyt\n"
              << "  mooncyt ast arquivo.mncyt\n"
              << "  moon --help\n";
}

enum class ValueKind {
    Integer,
    String,
    Char,
    Boolean,
    Bytes,
    Result
};

struct Value {
    ValueKind kind = ValueKind::Integer;
    std::string type_name = "Int32";
    long long int_val = 0;
    std::string str_val;
    char char_val = '\0';
    bool bool_val = false;
    std::vector<unsigned char> bytes_val;
    bool result_ok = false;
    std::shared_ptr<Value> result_value;

    static Value make_int(long long v, std::string type = "Int32") {
        Value val;
        val.kind = ValueKind::Integer;
        val.type_name = std::move(type);
        val.int_val = v;
        return val;
    }

    static Value make_string(std::string s, std::string type = "String32") {
        Value val;
        val.kind = ValueKind::String;
        val.type_name = std::move(type);
        val.str_val = std::move(s);
        return val;
    }

    static Value make_char(char c, std::string type = "Char") {
        Value val;
        val.kind = ValueKind::Char;
        val.type_name = std::move(type);
        val.char_val = c;
        return val;
    }

    static Value make_bool(bool v) {
        Value val;
        val.kind = ValueKind::Boolean;
        val.type_name = "Bool";
        val.bool_val = v;
        return val;
    }

    static Value make_bytes(std::vector<unsigned char> v) {
        Value val;
        val.kind = ValueKind::Bytes;
        val.type_name = "Bytes";
        val.bytes_val = std::move(v);
        return val;
    }

    static Value make_result_ok(Value value) {
        Value result;
        result.kind = ValueKind::Result;
        result.type_name = "Result";
        result.result_ok = true;
        result.result_value = std::make_shared<Value>(std::move(value));
        return result;
    }

    static Value make_result_error(Value value) {
        Value result;
        result.kind = ValueKind::Result;
        result.type_name = "Result";
        result.result_ok = false;
        result.result_value = std::make_shared<Value>(std::move(value));
        return result;
    }

    std::string to_string() const {
        switch (kind) {
            case ValueKind::Integer: return std::to_string(int_val);
            case ValueKind::String: return str_val;
            case ValueKind::Char: return std::string(1, char_val);
            case ValueKind::Boolean: return bool_val ? "true" : "false";
            case ValueKind::Bytes: {
                std::ostringstream out;
                for (const auto byte : bytes_val) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                return out.str();
            }
            case ValueKind::Result:
                return result_value ? result_value->to_string() : "";
        }
        return "";
    }
};

struct FunctionDef {
    std::string name;
    std::vector<std::pair<std::string, std::string>> params;
    std::string return_type = "Void";
    std::string body;
};

std::optional<long long> checked_add(long long a, long long b) {
    if ((b > 0 && a > std::numeric_limits<long long>::max() - b) ||
        (b < 0 && a < std::numeric_limits<long long>::min() - b)) return std::nullopt;
    return a + b;
}

std::optional<long long> checked_subtract(long long a, long long b) {
    if ((b < 0 && a > std::numeric_limits<long long>::max() + b) ||
        (b > 0 && a < std::numeric_limits<long long>::min() + b)) return std::nullopt;
    return a - b;
}

std::optional<long long> checked_multiply(long long a, long long b) {
    if (a == 0 || b == 0) return 0;
    if (a == -1 && b == std::numeric_limits<long long>::min()) return std::nullopt;
    if (b == -1 && a == std::numeric_limits<long long>::min()) return std::nullopt;
    if (a > 0) {
        if (b > 0 && a > std::numeric_limits<long long>::max() / b) return std::nullopt;
        if (b < 0 && b < std::numeric_limits<long long>::min() / a) return std::nullopt;
    } else {
        if (b > 0 && a < std::numeric_limits<long long>::min() / b) return std::nullopt;
        if (b < 0 && a < std::numeric_limits<long long>::max() / b) return std::nullopt;
    }
    return a * b;
}

std::optional<std::pair<long long, unsigned long long>> integer_type_range(const std::string& type_name) {
    unsigned bits = 0;
    if (type_name == "Int8" || type_name == "UInt8") bits = 8;
    else if (type_name == "Int16" || type_name == "UInt16") bits = 16;
    else if (type_name == "Int32" || type_name == "UInt32") bits = 32;
    else if (type_name == "Int64" || type_name == "UInt64") bits = 64;
    else return std::nullopt;

    if (type_name[0] == 'U') {
        const auto max = bits == 64 ? std::numeric_limits<unsigned long long>::max()
                                    : ((1ULL << bits) - 1ULL);
        return std::make_pair(0LL, max);
    }
    if (bits == 64) {
        return std::make_pair(std::numeric_limits<long long>::min(),
                              static_cast<unsigned long long>(std::numeric_limits<long long>::max()));
    }
    const long long max = (1LL << (bits - 1)) - 1;
    const long long min = -(1LL << (bits - 1));
    return std::make_pair(min, static_cast<unsigned long long>(max));
}

bool value_matches_type(const Value& value, const std::string& type_name) {
    if (type_name.empty() || type_name == "Inferred") return true;
    if (type_name == "Bool") return value.kind == ValueKind::Boolean;
    if (type_name == "Char") return value.kind == ValueKind::Char;
    if (type_name == "String" || type_name == "String32" || type_name == "String64") return value.kind == ValueKind::String;
    if (type_name == "Bytes") return value.kind == ValueKind::Bytes;
    if (type_name == "Result" || type_name.rfind("Result<", 0) == 0) return value.kind == ValueKind::Result;
    if (type_name.rfind("Int", 0) == 0 || type_name.rfind("UInt", 0) == 0) {
        if (value.kind != ValueKind::Integer) return false;
        const auto range = integer_type_range(type_name);
        if (!range) return false;
        if (value.int_val < range->first) return false;
        if (type_name[0] == 'U') return static_cast<unsigned long long>(value.int_val) <= range->second;
        return value.int_val <= static_cast<long long>(range->second);
    }
    return true;
}

std::optional<std::string> sha256_hex(const std::string& text) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return std::nullopt;
    DWORD hash_length = 0, result_length = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_length), sizeof(hash_length), &result_length, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0); return std::nullopt;
    }
    std::vector<unsigned char> digest(hash_length);
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) != 0 ||
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()), 0) != 0 ||
        BCryptFinishHash(hash, digest.data(), hash_length, 0) != 0) {
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0); return std::nullopt;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    std::ostringstream output;
    for (const auto byte : digest) output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return output.str();
#elif defined(MOONCRYPTO_USE_OPENSSL)
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) return std::nullopt;
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digest_length = 0;
    const bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1 &&
                    EVP_DigestUpdate(context, text.data(), text.size()) == 1 &&
                    EVP_DigestFinal_ex(context, digest, &digest_length) == 1;
    EVP_MD_CTX_free(context);
    if (!ok) return std::nullopt;
    std::ostringstream output;
    for (unsigned int i = 0; i < digest_length; ++i) {
        output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return output.str();
#else
    (void)text;
    return std::nullopt;
#endif
}

std::optional<std::string> hmac_sha256_hex(const std::string& text, const std::string& key) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return std::nullopt;
    DWORD hash_length = 0, result_length = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_length), sizeof(hash_length), &result_length, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0); return std::nullopt;
    }
    std::vector<unsigned char> digest(hash_length);
    auto* key_data = key.empty() ? nullptr : reinterpret_cast<PUCHAR>(const_cast<char*>(key.data()));
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, key_data,
                         static_cast<ULONG>(key.size()), 0) != 0 ||
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()), 0) != 0 ||
        BCryptFinishHash(hash, digest.data(), hash_length, 0) != 0) {
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0); return std::nullopt;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    std::ostringstream output;
    for (const auto byte : digest) output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return output.str();
#elif defined(MOONCRYPTO_USE_OPENSSL)
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digest_length = 0;
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(text.data()), text.size(), digest, &digest_length)) return std::nullopt;
    std::ostringstream output;
    for (unsigned int i = 0; i < digest_length; ++i) {
        output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return output.str();
#else
    (void)text; (void)key;
    return std::nullopt;
#endif
}

std::optional<std::vector<unsigned char>> secure_random_bytes(std::size_t size) {
    // Evita alocações abusivas vindas de uma entrada controlada pelo usuário.
    constexpr std::size_t max_random_request = 1024 * 1024;
    if (size > max_random_request) return std::nullopt;
    std::vector<unsigned char> output(size);
    if (size == 0) return output;
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, output.data(), static_cast<ULONG>(output.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return std::nullopt;
    }
    return output;
#elif defined(MOONCRYPTO_USE_OPENSSL)
    if (RAND_bytes(output.data(), static_cast<int>(output.size())) != 1) return std::nullopt;
    return output;
#else
    return std::nullopt;
#endif
}

class MiniInterpreter {
public:
    MiniInterpreter(std::string source, std::filesystem::path path)
        : source_(std::move(source)), path_(std::move(path)) {}

    bool run() {
        const auto ast = moon::parse_ast(moon::lex_source(source_));
        if (!ast.ok()) {
            error_ = true;
            std::cerr << path_.string() << ':' << ast.line << ':' << ast.column
                      << ": error: " << ast.error << '\n';
            return false;
        }
        if (!parse_program()) return false;
        auto it = functions_.find("main");
        if (it == functions_.end()) {
            return fail("no 'main' function found in " + path_.string());
        }
        auto res = call_function("main", {});
        if (!res) return false;
        if (res->kind == ValueKind::Integer) {
            exit_code_ = static_cast<int>(res->int_val);
        }
        for (const auto& message : messages_) std::cout << message << '\n';
        return true;
    }

    int exit_code() const { return exit_code_; }

    bool parse_program() {
        while (position_ < source_.size()) {
            whitespace();
            if (position_ >= source_.size()) break;

            if (source_[position_] == '#') {
                if (word("#external")) {
                    if (!parse_external()) return false;
                    continue;
                }
            }

            if (word("fn")) {
                if (!parse_function_def()) return false;
                continue;
            }

            if (word("let")) {
                if (!parse_let(true)) return false;
                continue;
            }

            if (word("const")) {
                if (!parse_let(true, true)) return false;
                continue;
            }

            if (word("struct") || word("class")) {
                whitespace();
                while (position_ < source_.size() && source_[position_] != '{') ++position_;
                if (position_ < source_.size()) {
                    int depth = 0;
                    do {
                        if (source_[position_] == '{') ++depth;
                        if (source_[position_] == '}') --depth;
                        ++position_;
                    } while (position_ < source_.size() && depth > 0);
                }
                continue;
            }

            ++position_;
        }
        return true;
    }

    bool run_fragment(std::unordered_map<std::string, Value> variables,
                      std::vector<std::string>& output) {
        variables_ = std::move(variables);
        if (!parse_block(true, false)) return false;
        output.insert(output.end(), messages_.begin(), messages_.end());
        return true;
    }

private:
    bool fail(const std::string& message) {
        error_ = true;
        std::size_t line = 1;
        std::size_t column = 1;
        const auto limit = std::min(position_, source_.size());
        for (std::size_t i = 0; i < limit; ++i) {
            if (source_[i] == '\n') { ++line; column = 1; }
            else { ++column; }
        }
        std::cerr << path_.string() << ':' << line << ':' << column
                  << ": error: " << message << '\n';
        return false;
    }

    static void trim(std::string& s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    }

    bool parse_external() {
        whitespace();
        std::string path_str;
        if (position_ < source_.size() && (source_[position_] == '"' || source_[position_] == '\'')) {
            const char quote = source_[position_++];
            const auto start = position_;
            while (position_ < source_.size() && source_[position_] != quote) ++position_;
            if (position_ >= source_.size()) return fail("unterminated string in #external");
            path_str = source_.substr(start, position_ - start);
            ++position_;
        } else {
            const auto start = position_;
            while (position_ < source_.size() && !std::isspace(static_cast<unsigned char>(source_[position_])) && source_[position_] != ';') {
                ++position_;
            }
            path_str = source_.substr(start, position_ - start);
        }
        whitespace();
        if (position_ < source_.size() && source_[position_] == ';') {
            ++position_;
        }
        trim(path_str);
        if (path_str.empty()) return fail("empty path in #external");

        std::filesystem::path target = path_str;
        std::filesystem::path resolved;

        std::vector<std::filesystem::path> search_paths;
        if (path_.has_parent_path()) {
            search_paths.push_back(path_.parent_path() / target);
            if (target.extension() != ".mncyt") {
                search_paths.push_back(path_.parent_path() / (target.string() + ".mncyt"));
            }
        }
        search_paths.push_back(target);
        if (target.extension() != ".mncyt") {
            search_paths.push_back(target.string() + ".mncyt");
        }

        bool found = false;
        for (const auto& sp : search_paths) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(sp, ec) && !ec) {
                resolved = std::filesystem::weakly_canonical(sp, ec);
                if (ec) resolved = sp;
                found = true;
                break;
            }
        }

        if (!found) {
            return fail("cannot find #external module: " + path_str);
        }

        std::string canon_key = resolved.string();
        if (loaded_modules_.find(canon_key) != loaded_modules_.end()) {
            return true;
        }
        loaded_modules_.insert(canon_key);

        std::ifstream input(resolved, std::ios::binary);
        if (!input) {
            return fail("cannot read #external file: " + resolved.string());
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();

        MiniInterpreter ext_interp(buffer.str(), resolved);
        ext_interp.loaded_modules_ = loaded_modules_;
        ext_interp.functions_ = functions_;
        ext_interp.variables_ = variables_;

        if (!ext_interp.parse_program()) {
            return false;
        }

        for (auto& [fn_name, fn_def] : ext_interp.functions_) {
            functions_[fn_name] = std::move(fn_def);
        }
        for (auto& [var_name, var_val] : ext_interp.variables_) {
            variables_[var_name] = std::move(var_val);
        }
        loaded_modules_ = std::move(ext_interp.loaded_modules_);
        messages_.insert(messages_.end(), ext_interp.messages_.begin(), ext_interp.messages_.end());

        return true;
    }

    bool parse_function_def() {
        whitespace();
        std::size_t name_start = position_;
        if (position_ >= source_.size() || !(std::isalpha(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
            return fail("expected function name after 'fn'");
        }
        while (position_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
            ++position_;
        }
        std::string fn_name = source_.substr(name_start, position_ - name_start);

        whitespace();
        char close_delim = ']';
        if (position_ < source_.size() && source_[position_] == '[') {
            close_delim = ']';
            ++position_;
        } else if (position_ < source_.size() && source_[position_] == '(') {
            close_delim = ')';
            ++position_;
        } else {
            return fail("expected '[' or '(' after function name");
        }

        std::vector<std::pair<std::string, std::string>> params;
        whitespace();
        while (position_ < source_.size() && source_[position_] != close_delim) {
            whitespace();
            std::size_t p_start = position_;
            while (position_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
                ++position_;
            }
            std::string p_name = source_.substr(p_start, position_ - p_start);
            trim(p_name);
            if (p_name.empty()) break;

            if (!expect(':')) return fail("expected ':' after parameter name '" + p_name + "'");
            whitespace();
            std::size_t t_start = position_;
            while (position_ < source_.size() && source_[position_] != ',' && source_[position_] != close_delim) {
                ++position_;
            }
            std::string p_type = source_.substr(t_start, position_ - t_start);
            trim(p_type);
            params.push_back({p_name, p_type});

            whitespace();
            if (position_ < source_.size() && source_[position_] == ',') {
                ++position_;
                whitespace();
            }
        }
        if (!expect(close_delim)) return fail(std::string("expected '") + close_delim + "' to close parameter list");

        whitespace();
        std::string ret_type = "Void";
        if (position_ + 1 < source_.size() && source_[position_] == '-' && source_[position_ + 1] == '>') {
            position_ += 2;
            whitespace();
            std::size_t r_start = position_;
            while (position_ < source_.size() && source_[position_] != '{') {
                ++position_;
            }
            ret_type = source_.substr(r_start, position_ - r_start);
            trim(ret_type);
        }

        if (!expect('{')) return fail("expected '{' for function body of '" + fn_name + "'");
        std::size_t body_start = position_;
        int depth = 1;
        while (position_ < source_.size() && depth > 0) {
            if (source_[position_] == '{') ++depth;
            else if (source_[position_] == '}') --depth;
            ++position_;
        }
        if (depth != 0) return fail("unterminated body in function '" + fn_name + "'");
        std::string body = source_.substr(body_start, position_ - body_start - 1);

        FunctionDef def;
        def.name = fn_name;
        def.params = std::move(params);
        def.return_type = ret_type;
        def.body = std::move(body);
        functions_[fn_name] = std::move(def);

        return true;
    }

    std::vector<Value> parse_argument_list(const std::string& text) {
        std::vector<Value> args;
        std::string current;
        bool in_d = false;
        bool in_s = false;
        bool esc = false;
        int depth_bracket = 0;
        int depth_paren = 0;

        for (std::size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (esc) { current.push_back(c); esc = false; continue; }
            if (c == '\\') { current.push_back(c); esc = true; continue; }
            if (c == '"' && !in_s) { in_d = !in_d; current.push_back(c); continue; }
            if (c == '\'' && !in_d) { in_s = !in_s; current.push_back(c); continue; }
            if (!in_d && !in_s) {
                if (c == '[') ++depth_bracket;
                else if (c == ']' && depth_bracket > 0) --depth_bracket;
                else if (c == '(') ++depth_paren;
                else if (c == ')' && depth_paren > 0) --depth_paren;
                else if (c == ',' && depth_bracket == 0 && depth_paren == 0) {
                    trim(current);
                    if (!current.empty()) {
                        auto val = evaluate_expression(current);
                        if (val) args.push_back(*val);
                    }
                    current.clear();
                    continue;
                }
            }
            current.push_back(c);
        }
        trim(current);
        if (!current.empty()) {
            auto val = evaluate_expression(current);
            if (val) args.push_back(*val);
        }
        return args;
    }

    std::optional<Value> call_function(const std::string& name, const std::vector<Value>& args) {
        auto it = functions_.find(name);
        if (it == functions_.end()) {
            fail("call to undefined function '" + name + "'");
            return std::nullopt;
        }
        const auto& def = it->second;
        if (args.size() != def.params.size()) {
            fail("function '" + name + "' expects " + std::to_string(def.params.size()) +
                 " arguments, but " + std::to_string(args.size()) + " were provided");
            return std::nullopt;
        }

        MiniInterpreter func_interp(def.body, path_);
        func_interp.functions_ = functions_;
        func_interp.loaded_modules_ = loaded_modules_;
        func_interp.variables_ = variables_;
        func_interp.constants_ = constants_;
        for (std::size_t i = 0; i < args.size(); ++i) {
            Value arg_val = args[i];
            arg_val.type_name = def.params[i].second;
            func_interp.variables_[def.params[i].first] = std::move(arg_val);
        }

        if (!func_interp.parse_block(true, false)) {
            return std::nullopt;
        }
        messages_.insert(messages_.end(), func_interp.messages_.begin(), func_interp.messages_.end());

        if (func_interp.returned_) {
            return func_interp.return_value_;
        }
        return Value::make_int(0);
    }

    bool parse_function_call_statement(bool active) {
        whitespace();
        if (position_ >= source_.size()) return false;
        const auto saved_pos = position_;
        if (!(std::isalpha(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
            return false;
        }
        std::size_t start = position_;
        while (position_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
            ++position_;
        }
        const std::string name = source_.substr(start, position_ - start);
        if (functions_.find(name) == functions_.end()) {
            position_ = saved_pos;
            return false;
        }
        whitespace();
        if (position_ >= source_.size() || source_[position_] != '[') {
            position_ = saved_pos;
            return false;
        }
        char open_c = source_[position_++];
        char close_c = ']';

        std::size_t arg_start = position_;
        int depth = 1;
        bool in_d = false;
        bool in_s = false;
        bool esc = false;
        while (position_ < source_.size() && depth > 0) {
            char c = source_[position_++];
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"' && !in_s) { in_d = !in_d; continue; }
            if (c == '\'' && !in_d) { in_s = !in_s; continue; }
            if (!in_d && !in_s) {
                if (c == open_c) ++depth;
                else if (c == close_c) --depth;
            }
        }
        if (depth != 0) return fail("unterminated argument list in call to '" + name + "'");
        std::string arg_content = source_.substr(arg_start, position_ - arg_start - 1);
        if (!expect(';')) return fail("expected ';' after function call to '" + name + "'");

        if (active) {
            auto args = parse_argument_list(arg_content);
            auto res = call_function(name, args);
            if (!res) return false;
        }
        return true;
    }

    static std::optional<std::string> unescape_string(const std::string& text) {
        if (text.size() < 2 || text.front() != '"') return std::nullopt;
        std::string result;
        std::size_t i = 1;
        while (i < text.size()) {
            char c = text[i++];
            if (c == '"') {
                while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
                if (i == text.size()) return result;
                return std::nullopt;
            }
            if (c == '\\') {
                if (i >= text.size()) return std::nullopt;
                char esc = text[i++];
                switch (esc) {
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case '\\': result.push_back('\\'); break;
                    case '"': result.push_back('"'); break;
                    default: result.push_back(esc); break;
                }
            } else {
                result.push_back(c);
            }
        }
        return std::nullopt;
    }

    static std::optional<char> unescape_char(const std::string& text) {
        if (text.size() < 3 || text.front() != '\'') return std::nullopt;
        std::size_t i = 1;
        char c = text[i++];
        if (c == '\\') {
            if (i >= text.size()) return std::nullopt;
            char esc = text[i++];
            switch (esc) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '\\': c = '\\'; break;
                case '\'': c = '\''; break;
                case '0': c = '\0'; break;
                default: c = esc; break;
            }
        }
        if (i >= text.size() || text[i++] != '\'') return std::nullopt;
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (i == text.size()) return c;
        return std::nullopt;
    }

    static std::vector<std::string> split_top_level_plus(const std::string& text) {
        std::vector<std::string> parts;
        std::string current;
        bool in_d = false;
        bool in_s = false;
        bool esc = false;
        int depth_bracket = 0;
        int depth_paren = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (esc) {
                current.push_back(c);
                esc = false;
                continue;
            }
            if (c == '\\') {
                current.push_back(c);
                esc = true;
                continue;
            }
            if (c == '"' && !in_s) {
                in_d = !in_d;
                current.push_back(c);
                continue;
            }
            if (c == '\'' && !in_d) {
                in_s = !in_s;
                current.push_back(c);
                continue;
            }
            if (!in_d && !in_s) {
                if (c == '[') ++depth_bracket;
                else if (c == ']' && depth_bracket > 0) --depth_bracket;
                else if (c == '(') ++depth_paren;
                else if (c == ')' && depth_paren > 0) --depth_paren;
                else if (c == '+' && depth_bracket == 0 && depth_paren == 0) {
                    parts.push_back(current);
                    current.clear();
                    continue;
                }
            }
            current.push_back(c);
        }
        parts.push_back(current);
        return parts;
    }

    static std::size_t find_outside_quotes(const std::string& text, const char* needle) {
        const std::size_t needle_len = std::strlen(needle);
        bool in_d = false;
        bool in_s = false;
        bool esc = false;
        for (std::size_t i = 0; i + needle_len <= text.size(); ++i) {
            char c = text[i];
            if (esc) {
                esc = false;
                continue;
            }
            if (c == '\\') {
                esc = true;
                continue;
            }
            if (c == '"' && !in_s) {
                in_d = !in_d;
                continue;
            }
            if (c == '\'' && !in_d) {
                in_s = !in_s;
                continue;
            }
            if (!in_d && !in_s) {
                if (text.compare(i, needle_len, needle) == 0) {
                    return i;
                }
            }
        }
        return std::string::npos;
    }

    void whitespace() {
        while (position_ < source_.size()) {
            if (std::isspace(static_cast<unsigned char>(source_[position_]))) {
                ++position_;
            } else if (source_.compare(position_, 2, "//") == 0) {
                while (position_ < source_.size() && source_[position_] != '\n') ++position_;
            } else {
                break;
            }
        }
    }

    bool word(const char* value) {
        const std::size_t length = std::strlen(value);
        if (source_.compare(position_, length, value) != 0) return false;
        const auto end = position_ + length;
        if (end < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[end])) || source_[end] == '_')) return false;
        position_ = end;
        return true;
    }

    bool expect(char value) {
        whitespace();
        if (position_ >= source_.size() || source_[position_] != value) return fail(std::string("expected '") + value + "'");
        ++position_;
        return true;
    }

    std::optional<long long> evaluate_integer_expression(std::string text) {
        trim(text);
        if (text.empty()) return std::nullopt;

        for (std::size_t i = text.size(); i > 0; --i) {
            const char c = text[i - 1];
            if (c == '+' || c == '-') {
                std::size_t prev = i - 1;
                while (prev > 0 && std::isspace(static_cast<unsigned char>(text[prev - 1]))) --prev;
                if (prev == 0) continue;
                const char prev_char = text[prev - 1];
                if (prev_char == '+' || prev_char == '-' || prev_char == '*' || prev_char == '/' || prev_char == '%' ||
                    prev_char == '&' || prev_char == '|' || prev_char == '^' || prev_char == '<' || prev_char == '>') continue;

                const auto left = evaluate_integer_expression(text.substr(0, i - 1));
                const auto right = evaluate_integer_expression(text.substr(i));
                if (!left || !right) return std::nullopt;
                return c == '+' ? checked_add(*left, *right) : checked_subtract(*left, *right);
            }
        }

        for (std::size_t i = text.size(); i > 0; --i) {
            const char c = text[i - 1];
            if (c == '*' || c == '/' || c == '%') {
                const auto left = evaluate_integer_expression(text.substr(0, i - 1));
                const auto right = evaluate_integer_expression(text.substr(i));
                if (!left || !right) return std::nullopt;
                if (c == '*') {
                    return checked_multiply(*left, *right);
                }
                if (*right == 0) return std::nullopt;
                if (c == '/' && *left == std::numeric_limits<long long>::min() && *right == -1) return std::nullopt;
                if (c == '/') return *left / *right;
                return *left % *right;
            }
        }

        for (const char* op : {"&", "^", "|"}) {
            for (std::size_t i = text.size(); i > 0; --i) {
                const auto at = i - 1;
                if (text[at] != op[0]) continue;
                if ((op[0] == '&' || op[0] == '|') && at + 1 < text.size() && text[at + 1] == op[0]) continue;
                const auto left = evaluate_integer_expression(text.substr(0, at));
                const auto right = evaluate_integer_expression(text.substr(at + 1));
                if (!left || !right) return std::nullopt;
                if (op[0] == '&') return *left & *right;
                if (op[0] == '^') return *left ^ *right;
                return *left | *right;
            }
        }

        for (const char* op : {"<<", ">>"}) {
            for (std::size_t i = text.size(); i >= 2; --i) {
                const auto at = i - 2;
                if (text.compare(at, 2, op) != 0) continue;
                const auto left = evaluate_integer_expression(text.substr(0, at));
                const auto right = evaluate_integer_expression(text.substr(at + 2));
                if (!left || !right || *right < 0 || *right >= 64) return std::nullopt;
                const auto shift = static_cast<unsigned>(*right);
                const auto bits = static_cast<unsigned long long>(*left);
                return op[0] == '<' ? static_cast<long long>(bits << shift)
                                     : static_cast<long long>(bits >> shift);
            }
        }

        if (text.front() == '~') {
            const auto value = evaluate_integer_expression(text.substr(1));
            if (value) return ~*value;
        }

        return integer_operand(text);
    }

    std::optional<Value> evaluate_expression(std::string text) {
        trim(text);
        if (text.empty()) return std::nullopt;

        auto plus_parts = split_top_level_plus(text);
        if (plus_parts.size() > 1) {
            bool has_string_or_char = false;
            std::vector<Value> part_values;
            part_values.reserve(plus_parts.size());
            bool all_ok = true;
            for (auto& p : plus_parts) {
                trim(p);
                auto val = evaluate_expression(p);
                if (!val) {
                    all_ok = false;
                    break;
                }
                if (val->kind == ValueKind::String || val->kind == ValueKind::Char) {
                    has_string_or_char = true;
                }
                part_values.push_back(*val);
            }
            if (all_ok && has_string_or_char) {
                std::string combined;
                for (const auto& pv : part_values) {
                    combined += pv.to_string();
                }
                return Value::make_string(combined);
            }
            if (all_ok && !has_string_or_char && !part_values.empty()) {
                long long sum = 0;
                for (const auto& pv : part_values) {
                    const auto next = checked_add(sum, pv.int_val);
                    if (!next) return std::nullopt;
                    sum = *next;
                }
                return Value::make_int(sum);
            }
        }

        if (text.size() > 8 && text.substr(text.size() - 8) == ".is_ok[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 8));
            if (value && value->kind == ValueKind::Result) return Value::make_bool(value->result_ok);
            return std::nullopt;
        }
        if (text.size() > 9 && text.substr(text.size() - 9) == ".unwrap[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 9));
            if (value && value->kind == ValueKind::Result && value->result_ok && value->result_value) return *value->result_value;
            return std::nullopt;
        }
        if (text.size() > 8 && text.substr(text.size() - 8) == ".error[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 8));
            if (value && value->kind == ValueKind::Result && !value->result_ok && value->result_value) return *value->result_value;
            return std::nullopt;
        }
        if (text.size() > 6 && text.substr(text.size() - 6) == ".hex[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 6));
            if (value) return Value::make_string(value->to_string());
        }

        auto unesc = unescape_string(text);
        if (unesc) {
            return Value::make_string(*unesc);
        }

        auto unesc_c = unescape_char(text);
        if (unesc_c) {
            return Value::make_char(*unesc_c);
        }

        if (text == "true") return Value::make_bool(true);
        if (text == "false") return Value::make_bool(false);

        if (text.rfind("ok[", 0) == 0 && text.back() == ']') {
            auto value = evaluate_expression(text.substr(3, text.size() - 4));
            if (!value) return std::nullopt;
            return Value::make_result_ok(*value);
        }
        if (text.rfind("err[", 0) == 0 && text.back() == ']') {
            auto value = evaluate_expression(text.substr(4, text.size() - 5));
            if (!value) return std::nullopt;
            return Value::make_result_error(*value);
        }

        if (text.rfind("Bytes[", 0) == 0 && text.back() == ']') {
            const auto values = parse_argument_list(text.substr(6, text.size() - 7));
            std::vector<unsigned char> bytes;
            for (const auto& value : values) {
                if (value.kind != ValueKind::Integer || value.int_val < 0 || value.int_val > 255) return std::nullopt;
                bytes.push_back(static_cast<unsigned char>(value.int_val));
            }
            return Value::make_bytes(std::move(bytes));
        }

        // Check for function call: name[...] or name(...)
        const auto open_pos = text.find('[');
        if (open_pos != std::string::npos && text.back() == ']') {
            std::string fn_name = text.substr(0, open_pos);
            trim(fn_name);
            if (fn_name == "crypto.sha256") {
                auto arg = evaluate_expression(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (!arg) return std::nullopt;
                auto digest = sha256_hex(arg->to_string());
                if (!digest) return std::nullopt;
                return Value::make_string(*digest, "Hash256");
            }
            if (fn_name == "crypto.hmac_sha256") {
                const auto args = parse_argument_list(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (args.size() != 2) return std::nullopt;
                auto digest = hmac_sha256_hex(args[0].to_string(), args[1].to_string());
                if (!digest) return std::nullopt;
                return Value::make_string(*digest, "Hash256");
            }
            if (fn_name == "crypto.random_bytes") {
                const auto count = evaluate_integer_expression(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (!count || *count < 0) return std::nullopt;
                auto bytes = secure_random_bytes(static_cast<std::size_t>(*count));
                if (!bytes) return std::nullopt;
                return Value::make_bytes(std::move(*bytes));
            }
            const auto variable = variables_.find(fn_name);
            if (variable != variables_.end() && variable->second.kind == ValueKind::Bytes) {
                auto index = evaluate_integer_expression(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (!index || *index < 0 || static_cast<std::size_t>(*index) >= variable->second.bytes_val.size()) return std::nullopt;
                return Value::make_int(variable->second.bytes_val[static_cast<std::size_t>(*index)], "UInt8");
            }
            if (functions_.find(fn_name) != functions_.end()) {
                std::string arg_content = text.substr(open_pos + 1, text.size() - open_pos - 2);
                auto args = parse_argument_list(arg_content);
                return call_function(fn_name, args);
            }
        }

        auto it = variables_.find(text);
        if (it != variables_.end()) {
            return it->second;
        }

        auto int_val = evaluate_integer_expression(text);
        if (int_val) {
            return Value::make_int(*int_val);
        }

        return std::nullopt;
    }

    std::optional<bool> evaluate_condition_text(std::string expression) {
        trim(expression);
        if (expression == "true") return true;
        if (expression == "false") return false;
        if (expression.size() >= 2 && expression.front() == '(' && expression.back() == ')') {
            expression = expression.substr(1, expression.size() - 2);
            trim(expression);
        }

        const auto or_pos = find_outside_quotes(expression, "||");
        if (or_pos != std::string::npos) {
            auto left = evaluate_condition_text(expression.substr(0, or_pos));
            auto right = evaluate_condition_text(expression.substr(or_pos + 2));
            if (!left || !right) return std::nullopt;
            return *left || *right;
        }
        const auto and_pos = find_outside_quotes(expression, "&&");
        if (and_pos != std::string::npos) {
            auto left = evaluate_condition_text(expression.substr(0, and_pos));
            auto right = evaluate_condition_text(expression.substr(and_pos + 2));
            if (!left || !right) return std::nullopt;
            return *left && *right;
        }
        if (!expression.empty() && expression.front() == '!') {
            auto value = evaluate_condition_text(expression.substr(1));
            if (!value) return std::nullopt;
            return !*value;
        }
        const char* operators[] = {"==", "!=", "<=", ">=", "<", ">"};
        for (const char* op : operators) {
            const auto at = find_outside_quotes(expression, op);
            if (at == std::string::npos) continue;
            const auto left = evaluate_expression(expression.substr(0, at));
            const auto right = evaluate_expression(expression.substr(at + std::strlen(op)));
            if (!left || !right) return std::nullopt;

            if (left->kind == ValueKind::String || right->kind == ValueKind::String) {
                const std::string& l = left->to_string();
                const std::string& r = right->to_string();
                if (std::string(op) == "==") return l == r;
                if (std::string(op) == "!=") return l != r;
                if (std::string(op) == "<=") return l <= r;
                if (std::string(op) == ">=") return l >= r;
                if (std::string(op) == "<") return l < r;
                return l > r;
            }
            if (left->kind == ValueKind::Char && right->kind == ValueKind::Char) {
                char l = left->char_val;
                char r = right->char_val;
                if (std::string(op) == "==") return l == r;
                if (std::string(op) == "!=") return l != r;
                if (std::string(op) == "<=") return l <= r;
                if (std::string(op) == ">=") return l >= r;
                if (std::string(op) == "<") return l < r;
                return l > r;
            }

            if (left->kind == ValueKind::Boolean && right->kind == ValueKind::Boolean) {
                if (std::string(op) == "==") return left->bool_val == right->bool_val;
                if (std::string(op) == "!=") return left->bool_val != right->bool_val;
                return std::nullopt;
            }

            const long long left_value = left->int_val;
            const long long right_value = right->int_val;
            if (std::string(op) == "==") return left_value == right_value;
            if (std::string(op) == "!=") return left_value != right_value;
            if (std::string(op) == "<=") return left_value <= right_value;
            if (std::string(op) == ">=") return left_value >= right_value;
            if (std::string(op) == "<") return left_value < right_value;
            return left_value > right_value;
        }
        // Uma condição simples também pode ser uma variável/literal Bool.
        // Inteiros não são convertidos implicitamente para Bool.
        if (const auto value = evaluate_expression(expression);
            value && value->kind == ValueKind::Boolean) {
            return value->bool_val;
        }
        return std::nullopt;
    }

    std::optional<bool> condition() {
        whitespace();
        const auto open = position_;
        if (position_ >= source_.size()) return std::nullopt;

        std::string expression;
        if (source_[position_] == '(') {
            ++position_;
            int depth = 1;
            while (position_ < source_.size() && depth > 0) {
                const char c = source_[position_++];
                if (c == '(') ++depth;
                else if (c == ')') --depth;
                if (depth > 0) expression.push_back(c);
            }
            if (depth != 0) {
                position_ = open;
                return std::nullopt;
            }
        } else if (source_[position_] == '[') {
            ++position_;
            int depth = 1;
            while (position_ < source_.size() && depth > 0) {
                const char c = source_[position_++];
                if (c == '[') ++depth;
                else if (c == ']') --depth;
                if (depth > 0) expression.push_back(c);
            }
            if (depth != 0) {
                position_ = open;
                return std::nullopt;
            }
        } else {
            while (position_ < source_.size() && source_[position_] != '{') {
                expression.push_back(source_[position_++]);
            }
        }

        const auto result = evaluate_condition_text(expression);
        if (!result) {
            position_ = open;
            return std::nullopt;
        }
        return result;
    }

    std::optional<long long> integer_operand(std::string text) const {
        trim(text);
        if (text.empty()) return std::nullopt;
        try {
            std::size_t used = 0;
            const long long value = std::stoll(text, &used);
            if (used == text.size()) return value;
        } catch (...) {
        }
        const auto found = variables_.find(text);
        if (found != variables_.end()) {
            if (found->second.kind == ValueKind::Integer) return found->second.int_val;
            if (found->second.kind == ValueKind::Char) return static_cast<long long>(found->second.char_val);
        }
        return std::nullopt;
    }

    bool parse_let(bool active, bool is_const = false) {
        whitespace();
        std::size_t start = position_;
        if (position_ >= source_.size() || !(std::isalpha(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) return false;
        ++position_;
        while (position_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) ++position_;
        const std::string name = source_.substr(start, position_ - start);
        whitespace();
        std::string type_name = "Inferred";
        if (position_ < source_.size() && source_[position_] == ':') {
            ++position_;
            whitespace();
            std::size_t type_start = position_;
            while (position_ < source_.size() && source_[position_] != '=') ++position_;
            type_name = source_.substr(type_start, position_ - type_start);
            trim(type_name);
        } else {
            while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
        }
        if (position_ >= source_.size()) return fail("expected '=' after let type annotation");
        if (!expect('=')) return false;
        whitespace();
        const std::size_t value_start = position_;
        while (position_ < source_.size() && source_[position_] != ';') ++position_;
        if (position_ >= source_.size()) return fail("invalid let declaration");
        const std::string expr = source_.substr(value_start, position_ - value_start);
        ++position_;

        if (active) {
            const auto val = evaluate_expression(expr);
            if (!val) return fail("invalid expression in let declaration of '" + name + "'");
            if (!value_matches_type(*val, type_name)) return fail("value does not match declared type '" + type_name + "' for '" + name + "'");
            Value v = *val;
            v.type_name = type_name;
            variables_[name] = std::move(v);
            if (is_const) constants_.insert(name);
        }
        return true;
    }

    bool parse_assignment(bool active) {
        whitespace();
        if (position_ >= source_.size()) return false;
        const auto saved_pos = position_;
        if (!(std::isalpha(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
            return false;
        }
        std::size_t start = position_;
        while (position_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
            ++position_;
        }
        const std::string name = source_.substr(start, position_ - start);
        if (constants_.find(name) != constants_.end()) {
            return fail("cannot assign to const variable '" + name + "'");
        }
        whitespace();
        if (position_ >= source_.size() || source_[position_] != '=') {
            position_ = saved_pos;
            return false;
        }
        if (position_ + 1 < source_.size() && source_[position_ + 1] == '=') {
            position_ = saved_pos;
            return false;
        }
        ++position_;
        whitespace();
        const std::size_t expr_start = position_;
        while (position_ < source_.size() && source_[position_] != ';') {
            ++position_;
        }
        if (position_ >= source_.size()) {
            return fail("unterminated assignment");
        }
        const std::string expr = source_.substr(expr_start, position_ - expr_start);
        ++position_;

        if (active) {
            const auto val = evaluate_expression(expr);
            if (!val) return fail("invalid expression in assignment to '" + name + "'");
            const auto it = variables_.find(name);
            if (it == variables_.end()) return fail("assignment to undeclared variable '" + name + "'");
            if (!value_matches_type(*val, it->second.type_name)) return fail("value does not match declared type '" + it->second.type_name + "' for '" + name + "'");
            Value v = *val;
            v.type_name = it->second.type_name;
            it->second = std::move(v);
        }
        return true;
    }

    bool parse_while(bool active) {
        if (!expect('[')) return false;
        const auto spec_start = position_;
        int depth = 1;
        while (position_ < source_.size() && depth > 0) {
            if (source_[position_] == '[') ++depth;
            else if (source_[position_] == ']') --depth;
            ++position_;
        }
        if (depth != 0) return fail("unterminated while condition");
        const std::string cond_expr = source_.substr(spec_start, position_ - spec_start - 1);
        if (!expect('{')) return false;
        const std::size_t body_start = position_;
        int body_depth = 1;
        while (position_ < source_.size() && body_depth > 0) {
            if (source_[position_] == '{') ++body_depth;
            else if (source_[position_] == '}') --body_depth;
            ++position_;
        }
        if (body_depth != 0) return fail("unterminated while body");
        const std::string body = source_.substr(body_start, position_ - body_start - 1);

        if (!active) return true;

        while (true) {
            const auto cond_val = evaluate_condition_text(cond_expr);
            if (!cond_val) return fail("invalid condition in while loop: " + cond_expr);
            if (!*cond_val) break;

            MiniInterpreter iteration(body, path_);
            iteration.functions_ = functions_;
            iteration.loaded_modules_ = loaded_modules_;
            iteration.variables_ = variables_;
            if (!iteration.parse_block(true, false)) return false;
            variables_ = std::move(iteration.variables_);
            messages_.insert(messages_.end(), iteration.messages_.begin(), iteration.messages_.end());
            if (iteration.returned_) {
                returned_ = true;
                return_value_ = iteration.return_value_;
                return true;
            }
            if (iteration.broke_) break;
        }
        return true;
    }

    bool parse_for(bool active) {
        if (!expect('[')) return false;
        const auto spec_start = position_;
        while (position_ < source_.size() && source_[position_] != ']') ++position_;
        if (position_ >= source_.size()) return fail("unterminated for clause");
        const std::string spec = source_.substr(spec_start, position_ - spec_start);
        ++position_;
        const auto first = spec.find(';');
        const auto second = first == std::string::npos ? std::string::npos : spec.find(';', first + 1);
        if (first == std::string::npos || second == std::string::npos) return fail("for requires initialization, condition and increment");
        const std::string init = spec.substr(0, first);
        const std::string test = spec.substr(first + 1, second - first - 1);
        const std::string step = spec.substr(second + 1);
        const auto let_pos = init.find("let ");
        const auto colon = init.find(':');
        const auto equals = init.find('=');
        if (let_pos == std::string::npos || colon == std::string::npos || equals == std::string::npos) return fail("invalid for initialization");
        const std::string name = init.substr(let_pos + 4, colon - let_pos - 4);
        const auto initial = integer_operand(init.substr(equals + 1));
        const auto test_value = test.find('<');
        const auto limit = test_value == std::string::npos ? std::optional<long long>{} : integer_operand(test.substr(test_value + 1));
        const auto plus = step.find('+');
        const auto increment = plus == std::string::npos ? std::optional<long long>{} : integer_operand(step.substr(plus + 1));
        if (!initial || !limit || !increment || test_value == std::string::npos) return fail("for currently supports integer '<' loops");
        if (!expect('{')) return false;
        const std::size_t body_start = position_;
        int depth = 1;
        while (position_ < source_.size() && depth > 0) {
            if (source_[position_] == '{') ++depth;
            else if (source_[position_] == '}') --depth;
            ++position_;
        }
        if (depth != 0) return fail("unterminated for body");
        const std::string body = source_.substr(body_start, position_ - body_start - 1);
        if (!active) return true;
        for (long long value = *initial; value < *limit; value += *increment) {
            MiniInterpreter iteration(body, path_);
            iteration.functions_ = functions_;
            iteration.loaded_modules_ = loaded_modules_;
            iteration.variables_ = variables_;
            iteration.variables_[name] = Value::make_int(value);
            if (!iteration.parse_block(true, false)) return false;
            variables_ = std::move(iteration.variables_);
            messages_.insert(messages_.end(), iteration.messages_.begin(), iteration.messages_.end());
            if (iteration.returned_) {
                returned_ = true;
                return_value_ = iteration.return_value_;
                return true;
            }
            if (iteration.broke_) break;
        }
        return true;
    }

    bool parse_switch(bool active) {
        // switch [expr] { case val { ... } case val2 { ... } default { ... } }
        if (!expect('[')) return false;
        const auto expr_start = position_;
        int depth = 1;
        while (position_ < source_.size() && depth > 0) {
            if (source_[position_] == '[') ++depth;
            else if (source_[position_] == ']') --depth;
            ++position_;
        }
        if (depth != 0) return fail("unterminated switch expression");
        const std::string switch_expr = source_.substr(expr_start, position_ - expr_start - 1);

        const auto switch_val = evaluate_expression(switch_expr);
        if (active && !switch_val) return fail("invalid expression in switch: " + switch_expr);

        if (!expect('{')) return false;

        bool matched = false;

        while (true) {
            whitespace();
            if (position_ >= source_.size()) return fail("unterminated switch body");
            if (source_[position_] == '}') {
                ++position_;
                break;
            }

            if (word("case")) {
                whitespace();
                // Collect case value expression until '{'
                const auto case_val_start = position_;
                while (position_ < source_.size() && source_[position_] != '{') ++position_;
                if (position_ >= source_.size()) return fail("expected '{' after case value");
                std::string case_expr = source_.substr(case_val_start, position_ - case_val_start);
                trim(case_expr);

                if (!expect('{')) return false;
                // Collect case body
                const auto body_start = position_;
                int body_depth = 1;
                while (position_ < source_.size() && body_depth > 0) {
                    if (source_[position_] == '{') ++body_depth;
                    else if (source_[position_] == '}') --body_depth;
                    ++position_;
                }
                if (body_depth != 0) return fail("unterminated case body");
                const std::string body = source_.substr(body_start, position_ - body_start - 1);

                if (!active || matched) continue;

                const auto case_val = evaluate_expression(case_expr);
                if (!case_val) return fail("invalid expression in case: " + case_expr);

                // Compare switch value with case value
                bool is_match = false;
                if (switch_val->kind == ValueKind::String || case_val->kind == ValueKind::String) {
                    is_match = (switch_val->to_string() == case_val->to_string());
                } else if (switch_val->kind == ValueKind::Char && case_val->kind == ValueKind::Char) {
                    is_match = (switch_val->char_val == case_val->char_val);
                } else {
                    is_match = (switch_val->int_val == case_val->int_val);
                }

                if (is_match) {
                    matched = true;
                    MiniInterpreter case_interp(body, path_);
                    case_interp.functions_ = functions_;
                    case_interp.loaded_modules_ = loaded_modules_;
                    case_interp.variables_ = variables_;
                    if (!case_interp.parse_block(true, false)) return false;
                    variables_ = std::move(case_interp.variables_);
                    messages_.insert(messages_.end(), case_interp.messages_.begin(), case_interp.messages_.end());
                    if (case_interp.returned_) {
                        returned_ = true;
                        return_value_ = case_interp.return_value_;
                        return true;
                    }
                    if (case_interp.broke_ || case_interp.continued_) {
                        broke_ = case_interp.broke_;
                        continued_ = case_interp.continued_;
                        return true;
                    }
                }
            } else if (word("default")) {
                if (!expect('{')) return false;
                const auto body_start = position_;
                int body_depth = 1;
                while (position_ < source_.size() && body_depth > 0) {
                    if (source_[position_] == '{') ++body_depth;
                    else if (source_[position_] == '}') --body_depth;
                    ++position_;
                }
                if (body_depth != 0) return fail("unterminated default body");
                const std::string body = source_.substr(body_start, position_ - body_start - 1);

                if (!active || matched) continue;

                matched = true;
                MiniInterpreter def_interp(body, path_);
                def_interp.functions_ = functions_;
                def_interp.loaded_modules_ = loaded_modules_;
                def_interp.variables_ = variables_;
                if (!def_interp.parse_block(true, false)) return false;
                variables_ = std::move(def_interp.variables_);
                messages_.insert(messages_.end(), def_interp.messages_.begin(), def_interp.messages_.end());
                if (def_interp.returned_) {
                    returned_ = true;
                    return_value_ = def_interp.return_value_;
                    return true;
                }
                if (def_interp.broke_ || def_interp.continued_) {
                    broke_ = def_interp.broke_;
                    continued_ = def_interp.continued_;
                    return true;
                }
            } else {
                return fail("expected 'case' or 'default' inside switch");
            }
        }
        return true;
    }

    bool parse_block(bool active, bool stop_at_close) {
        while (true) {
            whitespace();
            if (position_ >= source_.size()) return !stop_at_close;
            if (source_[position_] == '}') {
                ++position_;
                return true;
            }
            if (returned_ || broke_ || continued_) return true;
            if (word("break")) {
                if (!expect(';')) return false;
                if (active) {
                    broke_ = true;
                    return true;
                }
                continue;
            }
            if (word("continue")) {
                if (!expect(';')) return false;
                if (active) {
                    continued_ = true;
                    return true;
                }
                continue;
            }
            if (word("let")) {
                if (!parse_let(active)) return false;
                continue;
            }
            if (word("const")) {
                if (!parse_let(active, true)) return false;
                continue;
            }
            if (word("for")) {
                if (!parse_for(active)) return false;
                continue;
            }
            if (word("while")) {
                if (!parse_while(active)) return false;
                continue;
            }
            if (word("switch")) {
                if (!parse_switch(active)) return false;
                if (returned_ || broke_ || continued_) return true;
                continue;
            }
            if (word("if")) {
                const auto result = condition();
                if (!result || !expect('{')) return fail("invalid if statement");
                if (!parse_block(active && *result, true)) return false;
                if (returned_ || broke_ || continued_) return true;
                whitespace();
                const auto after_if = position_;
                if (word("else")) {
                    whitespace();
                    if (word("if")) {
                        const auto else_if_result = condition();
                        if (!else_if_result || !expect('{')) return fail("invalid else if statement");
                        if (!parse_block(active && !*result && *else_if_result, true)) return false;
                        whitespace();
                        if (word("else")) {
                            if (!expect('{')) return fail("expected '{' after else");
                            if (!parse_block(active && !*result && !*else_if_result, true)) return false;
                        }
                    } else {
                        if (!expect('{')) return fail("expected '{' after else");
                        if (!parse_block(active && !*result, true)) return false;
                    }
                    if (returned_ || broke_ || continued_) return true;
                } else {
                    position_ = after_if;
                }
                continue;
            }
            if (word("print")) {
                whitespace();
                if (position_ >= source_.size() || source_[position_] != '[') return fail("expected '[' after print");
                const char open = source_[position_++];
                const char close = ']';
                const auto arg_start = position_;
                int depth = 1;
                bool in_d = false;
                bool in_s = false;
                bool esc = false;
                while (position_ < source_.size() && depth > 0) {
                    char c = source_[position_++];
                    if (esc) {
                        esc = false;
                        continue;
                    }
                    if (c == '\\') {
                        esc = true;
                        continue;
                    }
                    if (c == '"' && !in_s) {
                        in_d = !in_d;
                        continue;
                    }
                    if (c == '\'' && !in_d) {
                        in_s = !in_s;
                        continue;
                    }
                    if (!in_d && !in_s) {
                        if (c == open) ++depth;
                        else if (c == close) --depth;
                    }
                }
                if (depth != 0) return fail("unterminated print argument");
                const std::string arg_expr = source_.substr(arg_start, position_ - arg_start - 1);
                if (!expect(';')) return fail("expected ';' after print");
                if (active) {
                    const auto val = evaluate_expression(arg_expr);
                    if (!val) return fail("invalid print expression: " + arg_expr);
                    messages_.push_back(val->to_string());
                }
                continue;
            }
            if (word("return")) {
                whitespace();
                const auto val_start = position_;
                while (position_ < source_.size() && source_[position_] != ';' && source_[position_] != '}' && source_[position_] != '\n') {
                    ++position_;
                }
                const std::string ret_expr = source_.substr(val_start, position_ - val_start);
                if (position_ < source_.size() && source_[position_] == ';') {
                    ++position_;
                }
                if (active) {
                    std::string trimmed = ret_expr;
                    trim(trimmed);
                    if (!trimmed.empty()) {
                        const auto val = evaluate_expression(trimmed);
                        if (!val) return fail("invalid expression in return: " + trimmed);
                        return_value_ = *val;
                    }
                    returned_ = true;
                    return true;
                }
                continue;
            }
            if (parse_function_call_statement(active)) {
                if (returned_ || broke_ || continued_) return true;
                continue;
            }
            if (parse_assignment(active)) {
                continue;
            }
            if (error_) return false;
            ++position_;
        }
    }

    std::string source_;
    std::filesystem::path path_;
    std::size_t position_ = 0;
    std::unordered_map<std::string, Value> variables_;
    std::unordered_set<std::string> constants_;
    std::unordered_map<std::string, FunctionDef> functions_;
    std::unordered_set<std::string> loaded_modules_;
    std::vector<std::string> messages_;
    Value return_value_;
    bool returned_ = false;
    bool broke_ = false;
    bool continued_ = false;
    bool error_ = false;
    int exit_code_ = 0;
};

int execute_print_subset(const std::filesystem::path& source_path) {
    std::ifstream input(source_path, std::ios::binary);
    if (!input) {
        std::cerr << "moon: error: cannot read source file: " << source_path.string() << '\n';
        return -1;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    MiniInterpreter interpreter(buffer.str(), source_path);
    if (!interpreter.run()) return -1;
    return interpreter.exit_code();
}

} // namespace

int main(int argc, char** argv) {
    configure_console_encoding();

    if (argc == 3 && std::string(argv[1]) == "lex") {
        std::ifstream input(argv[2], std::ios::binary);
        if (!input) { std::cerr << "mooncyt: error: cannot read source file\n"; return 1; }
        std::ostringstream buffer; buffer << input.rdbuf();
        for (const auto& token : moon::lex_source(buffer.str())) {
            std::cout << token.line << ':' << token.column << ' ' << moon::token_kind_name(token.kind);
            if (!token.text.empty()) std::cout << " " << token.text;
            std::cout << '\n';
        }
        return 0;
    }

    if (argc == 3 && std::string(argv[1]) == "ast") {
        std::ifstream input(argv[2], std::ios::binary);
        if (!input) { std::cerr << "mooncyt: error: cannot read source file\n"; return 1; }
        std::ostringstream buffer; buffer << input.rdbuf();
        const auto ast = moon::parse_ast(moon::lex_source(buffer.str()));
        if (!ast.ok()) {
            std::cerr << argv[2] << ':' << ast.line << ':' << ast.column << ": error: " << ast.error << '\n';
            return 1;
        }
        for (const auto& function : ast.program.functions) {
            std::cout << "fn " << function.name << "[";
            for (std::size_t i = 0; i < function.parameters.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << function.parameters[i].name << ": " << function.parameters[i].type;
            }
            std::cout << "] -> " << function.return_type << '\n';
        }
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        print_usage();
        return 0;
    }

    // Direct source-file mode: mooncyt program.mncyt
    if (argc == 2) {
        const std::filesystem::path source_path = argv[1];
        if (source_path.extension() == ".mncyt") {
            if (!std::filesystem::is_regular_file(source_path)) {
                std::cerr << "mooncyt: error: source file not found: " << source_path.string() << '\n';
                return 1;
            }
            const int result = execute_print_subset(source_path);
            return result < 0 ? 1 : result;
        }
    }

    if (argc < 2 || (std::string(argv[1]) != "entry" && std::string(argv[1]) != "run") || argc > 3) {
        print_usage();
        return 2;
    }

    const std::filesystem::path project = argc == 3 ? argv[2] : ".";
    const auto resolution = moon::resolve_entry(project);
    if (!resolution.ok()) {
        std::cerr << "moon: error: " << resolution.error << '\n';
        for (const auto& candidate : resolution.candidates) {
            std::cerr << "  - " << std::filesystem::relative(candidate, resolution.project_root).string() << '\n';
        }
        return 1;
    }

    if (std::string(argv[1]) == "run") {
        const int result = execute_print_subset(resolution.entry_file);
        return result < 0 ? 1 : result;
    }

    std::cout << std::filesystem::relative(resolution.entry_file, resolution.project_root).string() << '\n';
    return 0;
}
