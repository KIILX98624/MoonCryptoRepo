#include "project.hpp"
#include "lexer.hpp"
#include "ast.hpp"

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(MOONCRYPTO_USE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

#ifndef _WIN32
#include <dlfcn.h>
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
              << "  monc entry [project-directory]\n"
              << "  monc run [arquivo.monc ou pasta]\n"
              << "  monc run caminho\\para\\main.monc\n"
              << "  monc run caminho\\para\\projeto\\\n"
              << "  monc lex arquivo.monc\n"
              << "  monc ast arquivo.monc\n"
              << "  monc --help\n";
}


struct JsonNode {
    enum class Type { Null, Boolean, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean_value = false;
    std::string scalar;
    std::vector<std::shared_ptr<JsonNode>> array;
    std::map<std::string, std::shared_ptr<JsonNode>> object;
};

std::shared_ptr<JsonNode> make_json_null() {
    return std::make_shared<JsonNode>();
}

std::string json_quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<unsigned>(c) << std::dec;
                } else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}

std::string json_serialize(const std::shared_ptr<JsonNode>& node) {
    if (!node) return "null";
    switch (node->type) {
        case JsonNode::Type::Null: return "null";
        case JsonNode::Type::Boolean: return node->boolean_value ? "true" : "false";
        case JsonNode::Type::Number: return node->scalar;
        case JsonNode::Type::String: return json_quote(node->scalar);
        case JsonNode::Type::Array: {
            std::string out = "[";
            for (std::size_t i = 0; i < node->array.size(); ++i) {
                if (i) out += ",";
                out += json_serialize(node->array[i]);
            }
            return out + "]";
        }
        case JsonNode::Type::Object: {
            std::string out = "{";
            bool first = true;
            for (const auto& entry : node->object) {
                if (!first) out += ",";
                first = false;
                out += json_quote(entry.first) + ":" + json_serialize(entry.second);
            }
            return out + "}";
        }
    }
    return "null";
}

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    std::shared_ptr<JsonNode> parse() {
        skip_space();
        auto result = parse_value(0);
        skip_space();
        if (!result || position_ != input_.size()) return nullptr;
        return result;
    }

private:
    const std::string& input_;
    std::size_t position_ = 0;

    void skip_space() {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\n' ||
                input_[position_] == '\r' || input_[position_] == '\t')) ++position_;
    }

    static void append_utf8(std::string& out, unsigned int cp) {
        if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool read_hex4(unsigned int& value) {
        if (position_ + 4 > input_.size()) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = input_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    std::optional<std::string> parse_string() {
        if (position_ >= input_.size() || input_[position_++] != '"') return std::nullopt;
        std::string out;
        while (position_ < input_.size()) {
            const unsigned char c = static_cast<unsigned char>(input_[position_++]);
            if (c == '"') return out;
            if (c < 0x20) return std::nullopt;
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (position_ >= input_.size()) return std::nullopt;
            const char escaped = input_[position_++];
            switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned int cp = 0;
                    if (!read_hex4(cp)) return std::nullopt;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (position_ + 2 > input_.size() || input_[position_] != '\\' || input_[position_ + 1] != 'u') return std::nullopt;
                        position_ += 2;
                        unsigned int low = 0;
                        if (!read_hex4(low) || low < 0xDC00 || low > 0xDFFF) return std::nullopt;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) return std::nullopt;
                    append_utf8(out, cp);
                    break;
                }
                default: return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::shared_ptr<JsonNode> parse_value(unsigned depth) {
        if (depth > 128 || position_ >= input_.size()) return nullptr;
        skip_space();
        if (position_ >= input_.size()) return nullptr;
        const char c = input_[position_];

        if (c == '"') {
            auto text = parse_string();
            if (!text) return nullptr;
            auto node = std::make_shared<JsonNode>();
            node->type = JsonNode::Type::String;
            node->scalar = std::move(*text);
            return node;
        }
        if (c == '{') {
            ++position_;
            auto node = std::make_shared<JsonNode>();
            node->type = JsonNode::Type::Object;
            skip_space();
            if (position_ < input_.size() && input_[position_] == '}') { ++position_; return node; }
            while (position_ < input_.size()) {
                skip_space();
                auto key = parse_string();
                if (!key) return nullptr;
                skip_space();
                if (position_ >= input_.size() || input_[position_++] != ':') return nullptr;
                auto value = parse_value(depth + 1);
                if (!value) return nullptr;
                node->object[*key] = std::move(value);
                skip_space();
                if (position_ < input_.size() && input_[position_] == '}') { ++position_; return node; }
                if (position_ >= input_.size() || input_[position_++] != ',') return nullptr;
            }
            return nullptr;
        }
        if (c == '[') {
            ++position_;
            auto node = std::make_shared<JsonNode>();
            node->type = JsonNode::Type::Array;
            skip_space();
            if (position_ < input_.size() && input_[position_] == ']') { ++position_; return node; }
            while (position_ < input_.size()) {
                auto value = parse_value(depth + 1);
                if (!value) return nullptr;
                node->array.push_back(std::move(value));
                skip_space();
                if (position_ < input_.size() && input_[position_] == ']') { ++position_; return node; }
                if (position_ >= input_.size() || input_[position_++] != ',') return nullptr;
            }
            return nullptr;
        }
        if (input_.compare(position_, 4, "true") == 0 || input_.compare(position_, 5, "false") == 0) {
            auto node = std::make_shared<JsonNode>();
            node->type = JsonNode::Type::Boolean;
            node->boolean_value = input_[position_] == 't';
            position_ += node->boolean_value ? 4 : 5;
            return node;
        }
        if (input_.compare(position_, 4, "null") == 0) {
            position_ += 4;
            return make_json_null();
        }

        const std::size_t start = position_;
        if (input_[position_] == '-') ++position_;
        if (position_ >= input_.size()) return nullptr;
        if (input_[position_] == '0') ++position_;
        else {
            if (input_[position_] < '1' || input_[position_] > '9') return nullptr;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            const auto fraction = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (fraction == position_) return nullptr;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            const auto exponent = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (exponent == position_) return nullptr;
        }
        auto node = std::make_shared<JsonNode>();
        node->type = JsonNode::Type::Number;
        node->scalar = input_.substr(start, position_ - start);
        return node;
    }
};

std::string json_type_name(const std::shared_ptr<JsonNode>& node) {
    if (!node) return "null";
    switch (node->type) {
        case JsonNode::Type::Null: return "null";
        case JsonNode::Type::Boolean: return "boolean";
        case JsonNode::Type::Number: return "number";
        case JsonNode::Type::String: return "string";
        case JsonNode::Type::Array: return "array";
        case JsonNode::Type::Object: return "object";
    }
    return "null";
}


enum class ValueKind {
    Integer,
    Float,
    String,
    Char,
    Boolean,
    Bytes,
    ByteArray,
    Array,
    Json,
    Result,
    Pointer
};

struct Value {
    ValueKind kind = ValueKind::Integer;
    std::string type_name = "Int32";
    long long int_val = 0;
    double float_val = 0.0;
    std::string str_val;
    char char_val = '\0';
    bool bool_val = false;
    std::vector<unsigned char> bytes_val;
    std::shared_ptr<std::vector<unsigned char>> bytearray_val = std::make_shared<std::vector<unsigned char>>();
    std::shared_ptr<std::vector<Value>> array_val = std::make_shared<std::vector<Value>>();
    std::string array_element_type = "Inferred";
    std::shared_ptr<JsonNode> json_val;
    bool result_ok = false;
    std::shared_ptr<Value> result_value;
    void* pointer_val = nullptr;

    static Value make_int(long long v, std::string type = "Int32") {
        Value val;
        val.kind = ValueKind::Integer;
        val.type_name = std::move(type);
        val.int_val = v;
        return val;
    }

    static Value make_float(double v, std::string type = "Float64") {
        Value val;
        val.kind = ValueKind::Float;
        val.type_name = std::move(type);
        val.float_val = val.type_name == "Float32" || val.type_name == "Double32"
            ? static_cast<double>(static_cast<float>(v)) : v;
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

    static Value make_bytearray(std::vector<unsigned char> v) {
        Value val;
        val.kind = ValueKind::ByteArray;
        val.type_name = "ByteArray";
        val.bytearray_val = std::make_shared<std::vector<unsigned char>>(std::move(v));
        return val;
    }

    static Value make_array(std::vector<Value> v, std::string type = "Array") {
        Value val;
        val.kind = ValueKind::Array;
        val.type_name = std::move(type);
        val.array_val = std::make_shared<std::vector<Value>>(std::move(v));
        if (val.array_val && !val.array_val->empty()) val.array_element_type = val.array_val->front().type_name;
        return val;
    }

    static Value make_json(std::shared_ptr<JsonNode> value) {
        Value val;
        val.kind = ValueKind::Json;
        val.type_name = "JsonValue";
        val.json_val = std::move(value);
        return val;
    }

    static Value make_pointer(void* pointer) {
        Value val;
        val.kind = ValueKind::Pointer;
        val.type_name = "Pointer";
        val.pointer_val = pointer;
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
            case ValueKind::Float: {
                std::ostringstream out;
                out << std::setprecision(type_name == "Float32" || type_name == "Double32" ? 7 : 15)
                    << std::defaultfloat << float_val;
                return out.str();
            }
            case ValueKind::String: return str_val;
            case ValueKind::Char: return std::string(1, char_val);
            case ValueKind::Boolean: return bool_val ? "true" : "false";
            case ValueKind::Bytes: {
                std::ostringstream out;
                for (const auto byte : bytes_val) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                return out.str();
            }
            case ValueKind::ByteArray: {
                std::string out;
                if (bytearray_val) {
                    for (std::size_t i = 0; i < bytearray_val->size(); ++i) {
                        if (i) out += ",";
                        out += std::to_string((*bytearray_val)[i]);
                    }
                }
                return out;
            }
            case ValueKind::Array: {
                std::string out = "[";
                if (array_val) {
                    for (std::size_t i = 0; i < array_val->size(); ++i) {
                        if (i) out += ", ";
                        out += (*array_val)[i].to_string();
                    }
                }
                return out + "]";
            }
            case ValueKind::Json: return json_serialize(json_val);
            case ValueKind::Result:
                return result_value ? result_value->to_string() : "";
            case ValueKind::Pointer: {
                if (!pointer_val) return "NULL";
                std::ostringstream out;
                out << pointer_val;
                return out.str();
            }
        }
        return "";
    }
};

std::shared_ptr<JsonNode> value_to_json_node(const Value& value) {
    auto node = std::make_shared<JsonNode>();
    switch (value.kind) {
        case ValueKind::Json:
            return value.json_val ? value.json_val : make_json_null();
        case ValueKind::Integer:
        case ValueKind::Float:
            node->type = JsonNode::Type::Number;
            node->scalar = value.to_string();
            return node;
        case ValueKind::String:
            node->type = JsonNode::Type::String;
            node->scalar = value.str_val;
            return node;
        case ValueKind::Char:
            node->type = JsonNode::Type::String;
            node->scalar = std::string(1, value.char_val);
            return node;
        case ValueKind::Boolean:
            node->type = JsonNode::Type::Boolean;
            node->boolean_value = value.bool_val;
            return node;
        case ValueKind::Bytes:
            node->type = JsonNode::Type::Array;
            for (const auto byte : value.bytes_val) {
                auto item = std::make_shared<JsonNode>();
                item->type = JsonNode::Type::Number;
                item->scalar = std::to_string(static_cast<unsigned int>(byte));
                node->array.push_back(std::move(item));
            }
            return node;
        case ValueKind::ByteArray:
            node->type = JsonNode::Type::Array;
            if (value.bytearray_val) {
                for (const auto byte : *value.bytearray_val) {
                    auto item = std::make_shared<JsonNode>();
                    item->type = JsonNode::Type::Number;
                    item->scalar = std::to_string(static_cast<unsigned int>(byte));
                    node->array.push_back(std::move(item));
                }
            }
            return node;
        case ValueKind::Array:
            node->type = JsonNode::Type::Array;
            if (value.array_val) {
                for (const auto& item : *value.array_val) {
                    auto converted = value_to_json_node(item);
                    if (!converted) return nullptr;
                    node->array.push_back(std::move(converted));
                }
            }
            return node;
        case ValueKind::Result:
        case ValueKind::Pointer:
            return nullptr;
    }
    return nullptr;
}

struct FunctionDef {
    std::string name;
    std::vector<std::pair<std::string, std::string>> params;
    std::string return_type = "Void";
    std::string body;
};

struct CFunctionBinding {
    std::string header;
    std::string return_type;
    std::vector<std::string> parameter_types;
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
    if (type_name == "Float32" || type_name == "Double32" ||
        type_name == "Float64" || type_name == "Double64") return value.kind == ValueKind::Float;
    if (type_name == "Bool") return value.kind == ValueKind::Boolean;
    if (type_name == "Char") return value.kind == ValueKind::Char;
    if (type_name == "String" || type_name == "String32" || type_name == "String64") return value.kind == ValueKind::String;
    if (type_name == "Bytes") return value.kind == ValueKind::Bytes;
    if (type_name == "ByteArray" || type_name == "bytearray") return value.kind == ValueKind::ByteArray;
    if (type_name == "Array" || type_name.rfind("Array<", 0) == 0) {
        if (value.kind != ValueKind::Array || !value.array_val) return false;
        if (type_name.rfind("Array<", 0) == 0 && type_name.back() == '>') {
            const std::string element_type = type_name.substr(6, type_name.size() - 7);
            for (const auto& item : *value.array_val) if (!value_matches_type(item, element_type)) return false;
        }
        return true;
    }
    if (type_name == "Pointer" || type_name == "void*" || type_name == "char*" || type_name == "const char*") return value.kind == ValueKind::Pointer;
    if (type_name == "Json" || type_name == "JsonValue") return value.kind == ValueKind::Json;
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
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return std::nullopt;
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

std::optional<std::vector<unsigned char>> hex_to_bytes(const std::string& hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int hi = -1, lo = -1;
        char c1 = hex[i], c2 = hex[i + 1];
        if (c1 >= '0' && c1 <= '9') hi = c1 - '0';
        else if (c1 >= 'a' && c1 <= 'f') hi = c1 - 'a' + 10;
        else if (c1 >= 'A' && c1 <= 'F') hi = c1 - 'A' + 10;
        if (c2 >= '0' && c2 <= '9') lo = c2 - '0';
        else if (c2 >= 'a' && c2 <= 'f') lo = c2 - 'a' + 10;
        else if (c2 >= 'A' && c2 <= 'F') lo = c2 - 'A' + 10;
        if (hi < 0 || lo < 0) return std::nullopt;
        bytes.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return bytes;
}

std::optional<std::string> bytes_to_hex(const std::vector<unsigned char>& bytes) {
    std::ostringstream out;
    for (const auto b : bytes) {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return out.str();
}

std::optional<std::string> crypto_derive_key_hex(const std::string& secret, std::size_t key_len) {
    if (key_len == 0 || key_len > 64) key_len = 32;
    auto h1 = sha256_hex(secret + ":kdf:moon");
    if (!h1) return std::nullopt;
    if (key_len <= 32) {
        return h1->substr(0, key_len * 2);
    }
    auto h2 = sha256_hex(*h1 + ":next");
    if (!h2) return std::nullopt;
    std::string combined = *h1 + *h2;
    return combined.substr(0, key_len * 2);
}

std::optional<std::string> aes_gcm_encrypt_hex(const std::string& plaintext, const std::string& key_str) {
    std::vector<unsigned char> key_bytes;
    if (key_str.size() == 64) {
        auto maybe_bytes = hex_to_bytes(key_str);
        if (maybe_bytes && maybe_bytes->size() == 32) key_bytes = std::move(*maybe_bytes);
        else {
            auto derived = crypto_derive_key_hex(key_str, 32);
            if (!derived) return std::nullopt;
            auto db = hex_to_bytes(*derived);
            if (!db) return std::nullopt;
            key_bytes = std::move(*db);
        }
    } else {
        auto derived = crypto_derive_key_hex(key_str, 32);
        if (!derived) return std::nullopt;
        auto db = hex_to_bytes(*derived);
        if (!db) return std::nullopt;
        key_bytes = std::move(*db);
    }

    auto maybe_iv = secure_random_bytes(12);
    if (!maybe_iv) return std::nullopt;
    const auto& iv = *maybe_iv;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return std::nullopt;
    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return std::nullopt;
    }
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptGenerateSymmetricKey(alg, &hKey, nullptr, 0,
                                   key_bytes.data(), static_cast<ULONG>(key_bytes.size()), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return std::nullopt;
    }

    std::vector<unsigned char> tag(16);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    std::vector<unsigned char> nonce = iv;
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = static_cast<ULONG>(nonce.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    std::vector<unsigned char> ciphertext(plaintext.size());
    ULONG bytes_done = 0;
    NTSTATUS status = BCryptEncrypt(
        hKey,
        reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
        static_cast<ULONG>(plaintext.size()),
        &authInfo,
        nullptr, 0,
        ciphertext.data(),
        static_cast<ULONG>(ciphertext.size()),
        &bytes_done, 0
    );

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status != 0) return std::nullopt;

    std::vector<unsigned char> final_blob;
    final_blob.insert(final_blob.end(), iv.begin(), iv.end());
    final_blob.insert(final_blob.end(), tag.begin(), tag.end());
    final_blob.insert(final_blob.end(), ciphertext.begin(), ciphertext.end());
    return bytes_to_hex(final_blob);
#elif defined(MOONCRYPTO_USE_OPENSSL)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key_bytes.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }

    std::vector<unsigned char> ciphertext(plaintext.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()), static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    int extra_len = 0;
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &extra_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    std::vector<unsigned char> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> final_blob;
    final_blob.insert(final_blob.end(), iv.begin(), iv.end());
    final_blob.insert(final_blob.end(), tag.begin(), tag.end());
    final_blob.insert(final_blob.end(), ciphertext.begin(), ciphertext.end());
    return bytes_to_hex(final_blob);
#else
    return std::nullopt;
#endif
}

std::optional<std::string> aes_gcm_decrypt_hex(const std::string& cipher_hex, const std::string& key_str) {
    auto maybe_blob = hex_to_bytes(cipher_hex);
    if (!maybe_blob || maybe_blob->size() < 28) return std::nullopt; // 12 iv + 16 tag
    const auto& blob = *maybe_blob;
    std::vector<unsigned char> iv(blob.begin(), blob.begin() + 12);
    std::vector<unsigned char> tag(blob.begin() + 12, blob.begin() + 28);
    std::vector<unsigned char> ciphertext(blob.begin() + 28, blob.end());

    std::vector<unsigned char> key_bytes;
    if (key_str.size() == 64) {
        auto maybe_bytes = hex_to_bytes(key_str);
        if (maybe_bytes && maybe_bytes->size() == 32) key_bytes = std::move(*maybe_bytes);
        else {
            auto derived = crypto_derive_key_hex(key_str, 32);
            if (!derived) return std::nullopt;
            auto db = hex_to_bytes(*derived);
            if (!db) return std::nullopt;
            key_bytes = std::move(*db);
        }
    } else {
        auto derived = crypto_derive_key_hex(key_str, 32);
        if (!derived) return std::nullopt;
        auto db = hex_to_bytes(*derived);
        if (!db) return std::nullopt;
        key_bytes = std::move(*db);
    }

#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return std::nullopt;
    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return std::nullopt;
    }
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptGenerateSymmetricKey(alg, &hKey, nullptr, 0,
                                   key_bytes.data(), static_cast<ULONG>(key_bytes.size()), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return std::nullopt;
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    std::vector<unsigned char> nonce = iv;
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = static_cast<ULONG>(nonce.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    std::vector<unsigned char> plaintext(ciphertext.size());
    ULONG bytes_done = 0;
    NTSTATUS status = BCryptDecrypt(
        hKey,
        ciphertext.empty() ? nullptr : ciphertext.data(),
        static_cast<ULONG>(ciphertext.size()),
        &authInfo,
        nullptr, 0,
        plaintext.empty() ? nullptr : plaintext.data(),
        static_cast<ULONG>(plaintext.size()),
        &bytes_done, 0
    );

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status != 0) return std::nullopt;

    return std::string(plaintext.begin(), plaintext.begin() + bytes_done);
#elif defined(MOONCRYPTO_USE_OPENSSL)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx, nullptr, nullptr, key_bytes.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    std::vector<unsigned char> plaintext(ciphertext.size());
    int len = 0;
    if (!ciphertext.empty()) {
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return std::nullopt;
        }
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    int extra_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &extra_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt; // Tag authentication failed!
    }
    EVP_CIPHER_CTX_free(ctx);
    return std::string(plaintext.begin(), plaintext.begin() + len + extra_len);
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
        for (const auto& external : ast.program.externals) {
            for (const auto& function : external.c_functions) {
                CFunctionBinding binding;
                binding.header = external.path;
                binding.return_type = function.return_type;
                for (const auto& parameter : function.parameters) {
                    binding.parameter_types.push_back(parameter.type);
                }
                c_functions_[function.name] = std::move(binding);
            }
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

        // The AST pass in run() already collected the C prototypes.
        // Consume the native declaration block without treating its header as a .monc file.
        if (position_ < source_.size() && source_[position_] == '{') {
            ++position_;
            whitespace();
            if (!word("playCodeC")) return fail("expected playCodeC block inside #external");
            if (!expect('{')) return false;
            int depth = 1;
            while (position_ < source_.size() && depth > 0) {
                if (source_[position_] == '{') ++depth;
                else if (source_[position_] == '}') --depth;
                ++position_;
            }
            if (depth != 0) return fail("unterminated playCodeC block");
            if (!expect('}')) return false;
            loaded_modules_.insert("@c:" + path_str);
            return true;
        }

        // "json" é um módulo nativo do interpretador, não um arquivo .monc.
        if (path_str == "json" || path_str == "json.monc") {
            loaded_modules_.insert("@builtin:json");
            return true;
        }

        std::filesystem::path target = path_str;
        std::filesystem::path resolved;

        std::vector<std::filesystem::path> search_paths;
        if (path_.has_parent_path()) {
            search_paths.push_back(path_.parent_path() / target);
            if (target.extension() != ".monc") {
                search_paths.push_back(path_.parent_path() / (target.string() + ".monc"));
            }
        }
        search_paths.push_back(target);
        if (target.extension() != ".monc") {
            search_paths.push_back(target.string() + ".monc");
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

    std::optional<Value> call_c_function(const std::string& name, const std::vector<Value>& args) {
        const auto found = c_functions_.find(name);
        if (found == c_functions_.end()) return std::nullopt;
        const auto& binding = found->second;

        const bool is_math = binding.header == "math.h";
        const bool is_stdlib = binding.header == "stdlib.h";
        if (!is_math && !is_stdlib) {
            fail("C header '" + binding.header + "' is not supported yet");
            return std::nullopt;
        }
        if (binding.parameter_types.size() != args.size() || args.size() > 2) {
            fail("C function '" + name + "' has an unsupported argument count (maximum: 2)");
            return std::nullopt;
        }

        const bool float_signature = binding.return_type == "float" || binding.return_type == "double";
        const bool int_signature = binding.return_type == "int";
        const bool void_signature = binding.return_type == "void";
        const bool malloc_signature = is_stdlib && binding.return_type == "void*" && name == "malloc" &&
            args.size() == 1 && binding.parameter_types.size() == 1 && binding.parameter_types[0] == "size_t";
        const bool free_signature = is_stdlib && void_signature && name == "free" &&
            args.size() == 1 && binding.parameter_types.size() == 1 && binding.parameter_types[0] == "void*";
        const bool srand_signature =
            is_stdlib && void_signature && args.size() == 1 &&
            binding.parameter_types.size() == 1 &&
            binding.parameter_types[0] == "unsigned int";
        const bool string_pointer_signature =
            is_stdlib && int_signature && args.size() == 1 &&
            binding.parameter_types.size() == 1 &&
            (binding.parameter_types[0] == "char*" || binding.parameter_types[0] == "const char*");
        if ((!float_signature && !int_signature && !srand_signature && !malloc_signature && !free_signature) ||
            (is_math && !float_signature) ||
            (is_stdlib && !int_signature && !srand_signature && !malloc_signature && !free_signature)) {
            fail("C function '" + name + "' has an unsupported signature for header '" + binding.header + "'");
            return std::nullopt;
        }
        if (free_signature && args[0].kind != ValueKind::Pointer) {
            fail("free expects a Pointer or NULL");
            return std::nullopt;
        }
        if (!string_pointer_signature && !srand_signature && !malloc_signature && !free_signature) {
            for (const auto& type : binding.parameter_types) {
                if (type != binding.return_type) {
                    fail("C function '" + name + "' currently requires parameters matching its return type");
                    return std::nullopt;
                }
            }
        } else if (string_pointer_signature && args[0].kind != ValueKind::String) {
            fail("C function '" + name + "' expects a MonC String for its char* parameter");
            return std::nullopt;
        }

        double float_values[2] = {0.0, 0.0};
        int int_values[2] = {0, 0};
        unsigned int unsigned_int_value = 0;
        std::size_t allocation_size = 0;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (malloc_signature) {
                if (args[i].kind != ValueKind::Integer || args[i].int_val < 0 ||
                    static_cast<unsigned long long>(args[i].int_val) >
                        static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
                    fail("malloc expects a non-negative size_t-compatible integer");
                    return std::nullopt;
                }
                allocation_size = static_cast<std::size_t>(args[i].int_val);
                continue;
            }
            if (free_signature) continue;
            if (string_pointer_signature) {
                // The std::string storage remains alive for the duration of the C call;
                // c_str() supplies the null-terminated buffer expected by C.
                continue;
            }
            if (srand_signature) {
                if (args[i].kind != ValueKind::Integer || args[i].int_val < 0 ||
                    static_cast<unsigned long long>(args[i].int_val) >
                        static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max())) {
                    fail("C function 'srand' expects an integer in the unsigned int range");
                    return std::nullopt;
                }
                unsigned_int_value = static_cast<unsigned int>(args[i].int_val);
                continue;
            }
            if (float_signature) {
                if (args[i].kind == ValueKind::Float) float_values[i] = args[i].float_val;
                else if (args[i].kind == ValueKind::Integer) float_values[i] = static_cast<double>(args[i].int_val);
                else {
                    fail("C function '" + name + "' expects numeric arguments");
                    return std::nullopt;
                }
            } else {
                if (args[i].kind != ValueKind::Integer ||
                    args[i].int_val < std::numeric_limits<int>::min() ||
                    args[i].int_val > std::numeric_limits<int>::max()) {
                    fail("C function '" + name + "' expects Int32-compatible arguments");
                    return std::nullopt;
                }
                int_values[i] = static_cast<int>(args[i].int_val);
            }
        }

        double float_result = 0.0;
        int int_result = 0;
        void* pointer_result = nullptr;
#ifdef _WIN32
        // Keep the CRT modules loaded for the interpreter lifetime. Unloading
        // a dynamically loaded CRT after each call can reset its internal state,
        // making srand() appear ineffective when rand() is called later.
        static HMODULE math_library = nullptr;
        static HMODULE stdlib_library = nullptr;
        HMODULE& library = is_math ? math_library : stdlib_library;
        if (!library) {
            library = LoadLibraryA("ucrtbase.dll");
            if (!library) library = LoadLibraryA("msvcrt.dll");
        }
        if (!library) {
            fail("could not load the Windows C runtime for header '" + binding.header + "'");
            return std::nullopt;
        }
        FARPROC symbol = GetProcAddress(library, name.c_str());
        if (!symbol) {
            fail("C symbol '" + name + "' was not found in the Windows runtime");
            return std::nullopt;
        }
        if (malloc_signature) {
            using Fn = void*(__cdecl*)(std::size_t);
            pointer_result = reinterpret_cast<Fn>(symbol)(allocation_size);
        } else if (free_signature) {
            using Fn = void(__cdecl*)(void*);
            reinterpret_cast<Fn>(symbol)(args[0].pointer_val);
        } else if (float_signature && binding.return_type == "double") {
            if (args.size() == 1) { using Fn = double(__cdecl*)(double); float_result = reinterpret_cast<Fn>(symbol)(float_values[0]); }
            else if (args.size() == 2) { using Fn = double(__cdecl*)(double, double); float_result = reinterpret_cast<Fn>(symbol)(float_values[0], float_values[1]); }
            else { using Fn = double(__cdecl*)(); float_result = reinterpret_cast<Fn>(symbol)(); }
        } else if (float_signature) {
            const float a = static_cast<float>(float_values[0]), b = static_cast<float>(float_values[1]);
            float r = 0.0f;
            if (args.size() == 1) { using Fn = float(__cdecl*)(float); r = reinterpret_cast<Fn>(symbol)(a); }
            else if (args.size() == 2) { using Fn = float(__cdecl*)(float, float); r = reinterpret_cast<Fn>(symbol)(a, b); }
            else { using Fn = float(__cdecl*)(); r = reinterpret_cast<Fn>(symbol)(); }
            float_result = r;
        } else if (srand_signature) {
            using Fn = void(__cdecl*)(unsigned int);
            reinterpret_cast<Fn>(symbol)(unsigned_int_value);
        } else if (string_pointer_signature) {
            using Fn = int(__cdecl*)(const char*);
            int_result = reinterpret_cast<Fn>(symbol)(args[0].str_val.c_str());
        } else {
            if (args.size() == 1) { using Fn = int(__cdecl*)(int); int_result = reinterpret_cast<Fn>(symbol)(int_values[0]); }
            else if (args.size() == 2) { using Fn = int(__cdecl*)(int, int); int_result = reinterpret_cast<Fn>(symbol)(int_values[0], int_values[1]); }
            else { using Fn = int(__cdecl*)(); int_result = reinterpret_cast<Fn>(symbol)(); }
        }
#else
        // Cache handles instead of closing after every call so stateful C
        // library functions keep their state across MonC calls.
        static void* math_library = nullptr;
        static void* stdlib_library = nullptr;
        void*& library = is_math ? math_library : stdlib_library;
        if (!library) {
            if (is_math) {
                library = dlopen("libm.so.6", RTLD_NOW | RTLD_LOCAL);
                if (!library) library = dlopen("libm.so", RTLD_NOW | RTLD_LOCAL);
            } else {
                library = dlopen("libc.so.6", RTLD_NOW | RTLD_LOCAL);
                if (!library) library = dlopen("libc.so", RTLD_NOW | RTLD_LOCAL);
            }
        }
        if (!library) {
            fail("could not load native library for header '" + binding.header + "'");
            return std::nullopt;
        }
        void* symbol = dlsym(library, name.c_str());
        if (!symbol) {
            dlclose(library);
            fail("C symbol '" + name + "' was not found in the native library");
            return std::nullopt;
        }
        if (malloc_signature) {
            using Fn = void*(*)(std::size_t);
            pointer_result = reinterpret_cast<Fn>(symbol)(allocation_size);
        } else if (free_signature) {
            using Fn = void(*)(void*);
            reinterpret_cast<Fn>(symbol)(args[0].pointer_val);
        } else if (float_signature && binding.return_type == "double") {
            if (args.size() == 1) { using Fn = double(*)(double); float_result = reinterpret_cast<Fn>(symbol)(float_values[0]); }
            else if (args.size() == 2) { using Fn = double(*)(double, double); float_result = reinterpret_cast<Fn>(symbol)(float_values[0], float_values[1]); }
            else { using Fn = double(*)(); float_result = reinterpret_cast<Fn>(symbol)(); }
        } else if (float_signature) {
            const float a = static_cast<float>(float_values[0]), b = static_cast<float>(float_values[1]);
            float r = 0.0f;
            if (args.size() == 1) { using Fn = float(*)(float); r = reinterpret_cast<Fn>(symbol)(a); }
            else if (args.size() == 2) { using Fn = float(*)(float, float); r = reinterpret_cast<Fn>(symbol)(a, b); }
            else { using Fn = float(*)(); r = reinterpret_cast<Fn>(symbol)(); }
            float_result = r;
        } else if (srand_signature) {
            using Fn = void(*)(unsigned int);
            reinterpret_cast<Fn>(symbol)(unsigned_int_value);
        } else if (string_pointer_signature) {
            using Fn = int(*)(const char*);
            int_result = reinterpret_cast<Fn>(symbol)(args[0].str_val.c_str());
        } else {
            if (args.size() == 1) { using Fn = int(*)(int); int_result = reinterpret_cast<Fn>(symbol)(int_values[0]); }
            else if (args.size() == 2) { using Fn = int(*)(int, int); int_result = reinterpret_cast<Fn>(symbol)(int_values[0], int_values[1]); }
            else { using Fn = int(*)(); int_result = reinterpret_cast<Fn>(symbol)(); }
        }
#endif
        if (malloc_signature) {
            if (allocation_size != 0 && pointer_result == nullptr) {
                fail("malloc failed to allocate the requested memory");
                return std::nullopt;
            }
            return Value::make_pointer(pointer_result);
        }
        if (free_signature || srand_signature) return Value::make_int(0, "Void");
        if (int_signature) return Value::make_int(int_result, "Int32");
        return Value::make_float(float_result, binding.return_type == "float" ? "Float32" : "Float64");
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
        func_interp.c_functions_ = c_functions_;
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
        // Namespaced native calls such as json.set[...] are valid statements too.
        while (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            if (position_ >= source_.size() ||
                !(std::isalpha(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
                position_ = saved_pos;
                return false;
            }
            while (position_ < source_.size() &&
                   (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) {
                ++position_;
            }
        }
        const std::string name = source_.substr(start, position_ - start);
        const bool native_json_call = name.rfind("json.", 0) == 0;
        const auto method_dot = name.find('.');
        const std::string method_object = method_dot == std::string::npos ? std::string() : name.substr(0, method_dot);
        const auto method_variable = variables_.find(method_object);
        const bool bytearray_method_call = method_dot != std::string::npos &&
            method_variable != variables_.end() && method_variable->second.kind == ValueKind::ByteArray;
        const bool native_builtin_call = name == "all" || name == "any" || name == "ascii" ||
            name == "bin" || name == "bool" || name == "breakpoint" || name == "bytearray",
            name == "float" || name == "hash" || name == "hex" || name == "int" ||
            name == "len" || name == "max" || name == "min";
        const bool c_external_call = c_functions_.find(name) != c_functions_.end();
        if (functions_.find(name) == functions_.end() && !native_json_call && !bytearray_method_call && !native_builtin_call && !c_external_call) {
            position_ = saved_pos;
            return false;
        }
        whitespace();
        const char expected_open = c_external_call ? '(' : '[';
        if (position_ >= source_.size() || source_[position_] != expected_open) {
            position_ = saved_pos;
            return false;
        }
        char open_c = source_[position_++];
        char close_c = c_external_call ? ')' : ']';

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
            if (c_external_call) {
                auto args = parse_argument_list(arg_content);
                auto result = call_c_function(name, args);
                if (!result) return false;
            } else if (native_json_call || bytearray_method_call || native_builtin_call) {
                auto result = evaluate_expression(name + "[" + arg_content + "]");
                if (!result) return fail("invalid native function or method call '" + name + "'");
            } else {
                auto args = parse_argument_list(arg_content);
                auto res = call_function(name, args);
                if (!res) return false;
            }
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

    std::optional<double> floating_operand(std::string text) {
        trim(text);
        if (text.empty()) return std::nullopt;
        try {
            std::size_t used = 0;
            const double number = std::stod(text, &used);
            if (used == text.size()) return number;
        } catch (...) {}
        if (moved_variables_.find(text) != moved_variables_.end()) return std::nullopt;
        const auto found = variables_.find(text);
        if (found != variables_.end()) {
            if (found->second.kind == ValueKind::Float) return found->second.float_val;
            if (found->second.kind == ValueKind::Integer) return static_cast<double>(found->second.int_val);
            if (found->second.kind == ValueKind::Char) return static_cast<double>(found->second.char_val);
        }
        return std::nullopt;
    }

    bool is_float_expression(const std::string& text) const {
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(text[i]))) continue;
            std::size_t j = i;
            while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) ++j;
            if (j < text.size() && text[j] == '.' &&
                j + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[j + 1]))) return true;
            if (j < text.size() && (text[j] == 'e' || text[j] == 'E')) {
                ++j;
                if (j < text.size() && (text[j] == '+' || text[j] == '-')) ++j;
                if (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) return true;
            }
            i = j ? j - 1 : i;
        }
        for (const auto& [name, value] : variables_) {
            if (value.kind == ValueKind::Float && !name.empty() && text.find(name) != std::string::npos) return true;
        }
        return false;
    }

    std::optional<double> evaluate_float_expression(std::string text) {
        trim(text);
        if (text.empty()) return std::nullopt;

        // Remove a pair of parentheses only when it encloses the whole expression.
        if (text.front() == '(' && text.back() == ')') {
            int depth = 0;
            bool encloses_all = true;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (text[i] == '(') ++depth;
                else if (text[i] == ')') --depth;
                if (depth == 0 && i + 1 < text.size()) { encloses_all = false; break; }
            }
            if (encloses_all) return evaluate_float_expression(text.substr(1, text.size() - 2));
        }

        auto find_operator = [&](const std::string& ops) -> std::size_t {
            int paren_depth = 0;
            int bracket_depth = 0;
            bool in_double = false, in_single = false, escaped = false;
            std::size_t candidate = std::string::npos;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const char c = text[i];
                if (escaped) { escaped = false; continue; }
                if (c == '\\') { escaped = true; continue; }
                if (c == '"' && !in_single) { in_double = !in_double; continue; }
                if (c == '\'' && !in_double) { in_single = !in_single; continue; }
                if (in_double || in_single) continue;
                if (c == '(') { ++paren_depth; continue; }
                if (c == ')') { --paren_depth; continue; }
                if (c == '[') { ++bracket_depth; continue; }
                if (c == ']') { --bracket_depth; continue; }
                if (paren_depth != 0 || bracket_depth != 0 || ops.find(c) == std::string::npos) continue;
                if (i == 0) continue;
                const char previous = text[i - 1];
                if (previous == '+' || previous == '-' || previous == '*' || previous == '/' || previous == '%' ||
                    previous == 'e' || previous == 'E') continue;
                candidate = i;
            }
            return candidate;
        };

        std::size_t at = find_operator("+-");
        if (at != std::string::npos) {
            const auto left = evaluate_float_expression(text.substr(0, at));
            const auto right = evaluate_float_expression(text.substr(at + 1));
            if (!left || !right) return std::nullopt;
            return text[at] == '+' ? *left + *right : *left - *right;
        }

        at = find_operator("*/%");
        if (at != std::string::npos) {
            const auto left = evaluate_float_expression(text.substr(0, at));
            const auto right = evaluate_float_expression(text.substr(at + 1));
            if (!left || !right) return std::nullopt;
            if ((text[at] == '/' || text[at] == '%') && *right == 0.0) return std::nullopt;
            if (text[at] == '*') return *left * *right;
            if (text[at] == '/') return *left / *right;
            return std::fmod(*left, *right);
        }

        if (text.front() == '+' || text.front() == '-') {
            const auto value = evaluate_float_expression(text.substr(1));
            if (!value) return std::nullopt;
            return text.front() == '-' ? -*value : *value;
        }
        return floating_operand(text);
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
                const bool has_float = std::any_of(part_values.begin(), part_values.end(),
                    [](const Value& value) { return value.kind == ValueKind::Float; });
                if (has_float) {
                    double sum = 0.0;
                    for (const auto& pv : part_values) {
                        if (pv.kind == ValueKind::Float) sum += pv.float_val;
                        else if (pv.kind == ValueKind::Integer) sum += static_cast<double>(pv.int_val);
                        else return std::nullopt;
                    }
                    return Value::make_float(sum);
                }
                long long sum = 0;
                for (const auto& pv : part_values) {
                    if (pv.kind != ValueKind::Integer) return std::nullopt;
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

        const std::vector<std::pair<std::string, std::string>> json_integer_conversions = {
            {".asInt8[]", "Int8"}, {".asInt16[]", "Int16"}, {".asInt32[]", "Int32"}, {".asInt64[]", "Int64"},
            {".asUInt8[]", "UInt8"}, {".asUInt16[]", "UInt16"}, {".asUInt32[]", "UInt32"}, {".asUInt64[]", "UInt64"},
            {".as_int[]", "Int32"}
        };
        for (const auto& conversion : json_integer_conversions) {
            const auto& suffix = conversion.first;
            if (text.size() > suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
                auto value = evaluate_expression(text.substr(0, text.size() - suffix.size()));
                if (!value || value->kind != ValueKind::Json || !value->json_val ||
                    value->json_val->type != JsonNode::Type::Number) return std::nullopt;
                try {
                    std::size_t used = 0;
                    const bool is_unsigned = conversion.second.rfind("UInt", 0) == 0;
                    long long number = 0;
                    if (is_unsigned) {
                        const auto unsigned_number = std::stoull(value->json_val->scalar, &used);
                        if (unsigned_number > static_cast<unsigned long long>(std::numeric_limits<long long>::max())) return std::nullopt;
                        number = static_cast<long long>(unsigned_number);
                    } else {
                        number = std::stoll(value->json_val->scalar, &used);
                    }
                    if (used != value->json_val->scalar.size()) return std::nullopt;
                    const auto range = integer_type_range(conversion.second);
                    if (!range || number < range->first) return std::nullopt;
                    if (is_unsigned) {
                        if (static_cast<unsigned long long>(number) > range->second) return std::nullopt;
                    } else if (number > static_cast<long long>(range->second)) {
                        return std::nullopt;
                    }
                    return Value::make_int(number, conversion.second);
                } catch (...) { return std::nullopt; }
            }
        }

        const std::vector<std::pair<std::string, std::string>> json_float_conversions = {
            {".asFloat32[]", "Float32"}, {".asFloat64[]", "Float64"},
            {".asDouble32[]", "Double32"}, {".asDouble64[]", "Double64"}
        };
        for (const auto& conversion : json_float_conversions) {
            const auto& suffix = conversion.first;
            if (text.size() > suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
                auto value = evaluate_expression(text.substr(0, text.size() - suffix.size()));
                if (!value || value->kind != ValueKind::Json || !value->json_val ||
                    value->json_val->type != JsonNode::Type::Number) return std::nullopt;
                try {
                    std::size_t used = 0;
                    const double number = std::stod(value->json_val->scalar, &used);
                    if (used != value->json_val->scalar.size() || !std::isfinite(number)) return std::nullopt;
                    return Value::make_float(number, conversion.second);
                } catch (...) { return std::nullopt; }
            }
        }

        if (text.size() > 11 && text.substr(text.size() - 11) == ".asString[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 11));
            if (value && value->kind == ValueKind::Json && value->json_val &&
                value->json_val->type == JsonNode::Type::String) return Value::make_string(value->json_val->scalar);
            return std::nullopt;
        }
        if (text.size() > 12 && text.substr(text.size() - 12) == ".as_string[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 12));
            if (value && value->kind == ValueKind::Json && value->json_val &&
                value->json_val->type == JsonNode::Type::String) return Value::make_string(value->json_val->scalar);
            return std::nullopt;
        }
        if (text.size() > 9 && text.substr(text.size() - 9) == ".asBool[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 9));
            if (value && value->kind == ValueKind::Json && value->json_val &&
                value->json_val->type == JsonNode::Type::Boolean) return Value::make_bool(value->json_val->boolean_value);
            return std::nullopt;
        }
        if (text.size() > 10 && text.substr(text.size() - 10) == ".as_bool[]") {
            auto value = evaluate_expression(text.substr(0, text.size() - 10));
            if (value && value->kind == ValueKind::Json && value->json_val &&
                value->json_val->type == JsonNode::Type::Boolean) return Value::make_bool(value->json_val->boolean_value);
            return std::nullopt;
        }

        auto unesc = unescape_string(text);
        if (unesc) {
            return Value::make_string(*unesc);
        }

        auto unesc_c = unescape_char(text);
        if (unesc_c) {
            return Value::make_char(*unesc_c);
        }

        if (text == "NULL") return Value::make_pointer(nullptr);
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
            const std::string arg_content = text.substr(open_pos + 1, text.size() - open_pos - 2);
            std::string fn_name = text.substr(0, open_pos);
            trim(fn_name);

            if (fn_name == "float" || fn_name == "int" || fn_name == "hex" ||
                fn_name == "hash" || fn_name == "len" || fn_name == "max" || fn_name == "min") {
                auto args = parse_argument_list(arg_content);
                if (fn_name == "float" || fn_name == "int" || fn_name == "hex" || fn_name == "hash" || fn_name == "len") {
                    if (args.size() != 1) return std::nullopt;
                    const Value& value = args[0];
                    if (fn_name == "float") {
                        if (value.kind == ValueKind::Integer) return Value::make_float(static_cast<double>(value.int_val), "Float64");
                        if (value.kind == ValueKind::Float) return Value::make_float(value.float_val, "Float64");
                        if (value.kind == ValueKind::Boolean) return Value::make_float(value.bool_val ? 1.0 : 0.0, "Float64");
                        if (value.kind == ValueKind::String) {
                            std::string input = value.str_val; trim(input);
                            if (input.empty()) return std::nullopt;
                            try { std::size_t used = 0; const double parsed = std::stod(input, &used); if (used != input.size()) return std::nullopt; return Value::make_float(parsed, "Float64"); }
                            catch (...) { return std::nullopt; }
                        }
                        return std::nullopt;
                    }
                    if (fn_name == "int") {
                        if (value.kind == ValueKind::Integer) return Value::make_int(value.int_val, "Int64");
                        if (value.kind == ValueKind::Boolean) return Value::make_int(value.bool_val ? 1 : 0, "Int64");
                        if (value.kind == ValueKind::Float) {
                            if (!std::isfinite(value.float_val)) return std::nullopt;
                            const long double number = static_cast<long double>(value.float_val);
                            if (number < static_cast<long double>(std::numeric_limits<long long>::min()) ||
                                number >= static_cast<long double>(std::numeric_limits<long long>::max())) return std::nullopt;
                            return Value::make_int(static_cast<long long>(value.float_val), "Int64");
                        }
                        if (value.kind == ValueKind::String) {
                            std::string input = value.str_val; trim(input);
                            if (input.empty()) return std::nullopt;
                            try { std::size_t used = 0; const long long parsed = std::stoll(input, &used, 10); if (used != input.size()) return std::nullopt; return Value::make_int(parsed, "Int64"); }
                            catch (...) { return std::nullopt; }
                        }
                        return std::nullopt;
                    }
                    if (fn_name == "hex") {
                        if (value.kind != ValueKind::Integer) return std::nullopt;
                        const long long number = value.int_val;
                        const bool negative = number < 0;
                        const unsigned long long magnitude = negative ? static_cast<unsigned long long>(-(number + 1)) + 1ULL : static_cast<unsigned long long>(number);
                        std::ostringstream out; out << (negative ? "-0x" : "0x") << std::hex << magnitude;
                        return Value::make_string(out.str());
                    }
                    if (fn_name == "hash") {
                        if (value.kind != ValueKind::Integer && value.kind != ValueKind::Float && value.kind != ValueKind::String &&
                            value.kind != ValueKind::Char && value.kind != ValueKind::Boolean && value.kind != ValueKind::Bytes) return std::nullopt;
                        const std::string material = value.type_name + ":" + value.to_string();
                        unsigned long long hash_value = 14695981039346656037ULL;
                        for (const unsigned char byte : material) { hash_value ^= byte; hash_value *= 1099511628211ULL; }
                        return Value::make_int(static_cast<long long>(hash_value & 0x7fffffffffffffffULL), "Int64");
                    }
                    if (fn_name == "len") {
                        std::size_t length = 0;
                        switch (value.kind) {
                            case ValueKind::String:
                                for (const unsigned char byte : value.str_val) if ((byte & 0xC0) != 0x80) ++length;
                                break;
                            case ValueKind::Array: if (!value.array_val) return std::nullopt; length = value.array_val->size(); break;
                            case ValueKind::ByteArray: if (!value.bytearray_val) return std::nullopt; length = value.bytearray_val->size(); break;
                            case ValueKind::Bytes: length = value.bytes_val.size(); break;
                            case ValueKind::Json:
                                if (!value.json_val) return std::nullopt;
                                switch (value.json_val->type) {
                                    case JsonNode::Type::String: for (const unsigned char byte : value.json_val->scalar) if ((byte & 0xC0) != 0x80) ++length; break;
                                    case JsonNode::Type::Array: length = value.json_val->array.size(); break;
                                    case JsonNode::Type::Object: length = value.json_val->object.size(); break;
                                    default: return std::nullopt;
                                }
                                break;
                            default: return std::nullopt;
                        }
                        if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
                        return Value::make_int(static_cast<long long>(length), "Int32");
                    }
                }
                if (fn_name == "max" || fn_name == "min") {
                    std::vector<Value> candidates;
                    if (args.size() == 1) {
                        const Value& source = args.front();
                        if (source.kind == ValueKind::Array && source.array_val) candidates = *source.array_val;
                        else if (source.kind == ValueKind::ByteArray && source.bytearray_val) {
                            for (const auto byte : *source.bytearray_val) candidates.push_back(Value::make_int(byte, "UInt8"));
                        } else if (source.kind == ValueKind::Bytes) {
                            for (const auto byte : source.bytes_val) candidates.push_back(Value::make_int(byte, "UInt8"));
                        } else return std::nullopt;
                    } else if (!args.empty()) candidates = args;
                    else return std::nullopt;
                    if (candidates.empty()) return std::nullopt;
                    auto compare = [](const Value& left, const Value& right) -> std::optional<int> {
                        if ((left.kind == ValueKind::Integer || left.kind == ValueKind::Float) &&
                            (right.kind == ValueKind::Integer || right.kind == ValueKind::Float)) {
                            if (left.kind == ValueKind::Integer && right.kind == ValueKind::Integer)
                                return left.int_val < right.int_val ? -1 : (left.int_val > right.int_val ? 1 : 0);
                            const double a = left.kind == ValueKind::Integer ? static_cast<double>(left.int_val) : left.float_val;
                            const double b = right.kind == ValueKind::Integer ? static_cast<double>(right.int_val) : right.float_val;
                            if (std::isnan(a) || std::isnan(b)) return std::nullopt;
                            return a < b ? -1 : (a > b ? 1 : 0);
                        }
                        if (left.kind == ValueKind::String && right.kind == ValueKind::String)
                            return left.str_val < right.str_val ? -1 : (left.str_val > right.str_val ? 1 : 0);
                        if (left.kind == ValueKind::Char && right.kind == ValueKind::Char)
                            return left.char_val < right.char_val ? -1 : (left.char_val > right.char_val ? 1 : 0);
                        if (left.kind == ValueKind::Boolean && right.kind == ValueKind::Boolean)
                            return left.bool_val == right.bool_val ? 0 : (left.bool_val ? 1 : -1);
                        return std::nullopt;
                    };
                    Value selected = candidates.front();
                    for (std::size_t i = 1; i < candidates.size(); ++i) {
                        const auto order = compare(candidates[i], selected);
                        if (!order) return std::nullopt;
                        if ((fn_name == "max" && *order > 0) || (fn_name == "min" && *order < 0)) selected = candidates[i];
                    }
                    return selected;
                }
            }

            if (fn_name == "bytearray") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1) return std::nullopt;
                std::vector<unsigned char> data;
                if (args[0].kind == ValueKind::Integer) {
                    if (args[0].int_val < 0 || args[0].int_val > 1000000) return std::nullopt;
                    data.resize(static_cast<std::size_t>(args[0].int_val), 0);
                } else if (args[0].kind == ValueKind::Array && args[0].array_val) {
                    for (const auto& item : *args[0].array_val) {
                        if (item.kind != ValueKind::Integer || item.int_val < 0 || item.int_val > 255) return std::nullopt;
                        data.push_back(static_cast<unsigned char>(item.int_val));
                    }
                } else if (args[0].kind == ValueKind::ByteArray && args[0].bytearray_val) {
                    data = *args[0].bytearray_val;
                } else return std::nullopt;
                return Value::make_bytearray(std::move(data));
            }
            if (fn_name == "all" || fn_name == "any" || fn_name == "bool") {
                auto args = parse_argument_list(arg_content);
                auto truthy = [](const Value& v) {
                    switch (v.kind) {
                        case ValueKind::Boolean: return v.bool_val;
                        case ValueKind::Integer: return v.int_val != 0;
                        case ValueKind::Float: return v.float_val != 0.0;
                        case ValueKind::String: return !v.str_val.empty();
                        case ValueKind::Char: return v.char_val != '\0';
                        case ValueKind::ByteArray: return v.bytearray_val && !v.bytearray_val->empty();
                        case ValueKind::Bytes: return !v.bytes_val.empty();
                        case ValueKind::Array: return v.array_val && !v.array_val->empty();
                        case ValueKind::Json:
                            if (!v.json_val) return false;
                            switch (v.json_val->type) {
                                case JsonNode::Type::Null: return false;
                                case JsonNode::Type::Boolean: return v.json_val->boolean_value;
                                case JsonNode::Type::Number:
                                    try { return std::stod(v.json_val->scalar) != 0.0; } catch (...) { return false; }
                                case JsonNode::Type::String: return !v.json_val->scalar.empty();
                                case JsonNode::Type::Array: return !v.json_val->array.empty();
                                case JsonNode::Type::Object: return !v.json_val->object.empty();
                            }
                            return false;
                        case ValueKind::Pointer: return v.pointer_val != nullptr;
                        case ValueKind::Result: return v.result_ok;
                    }
                    return false;
                };
                if (fn_name == "bool") {
                    if (args.size() != 1) return std::nullopt;
                    return Value::make_bool(truthy(args[0]));
                }
                if (args.size() != 1) return std::nullopt;
                const Value& sequence = args[0];
                std::vector<Value> values;
                if (sequence.kind == ValueKind::Array && sequence.array_val) values = *sequence.array_val;
                else if (sequence.kind == ValueKind::ByteArray && sequence.bytearray_val) {
                    for (auto b : *sequence.bytearray_val) values.push_back(Value::make_int(b, "UInt8"));
                } else if (sequence.kind == ValueKind::String) {
                    return Value::make_bool(fn_name == "all" ? true : !sequence.str_val.empty());
                } else return std::nullopt;
                bool result = fn_name == "all";
                for (const auto& value : values) {
                    if (fn_name == "all" && !truthy(value)) { result = false; break; }
                    if (fn_name == "any" && truthy(value)) { result = true; break; }
                }
                return Value::make_bool(result);
            }
            if (fn_name == "bin") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1 || (args[0].kind != ValueKind::Integer && args[0].kind != ValueKind::Float)) return std::nullopt;
                if (args[0].kind == ValueKind::Float &&
                    (args[0].float_val > static_cast<double>(std::numeric_limits<long long>::max()) ||
                     args[0].float_val < static_cast<double>(std::numeric_limits<long long>::min()))) return std::nullopt;
                long long n = args[0].kind == ValueKind::Integer ? args[0].int_val :
                    static_cast<long long>(std::round(args[0].float_val));
                if (n == 0) return Value::make_string("0b0");
                const bool negative = n < 0;
                unsigned long long magnitude = negative
                    ? static_cast<unsigned long long>(-(n + 1)) + 1ULL
                    : static_cast<unsigned long long>(n);
                std::string bits;
                while (magnitude) { bits.insert(bits.begin(), (magnitude & 1ULL) ? '1' : '0'); magnitude >>= 1; }
                return Value::make_string(std::string(negative ? "-0b" : "0b") + bits);
            }
            if (fn_name == "ascii") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1) return std::nullopt;
                if (args[0].kind != ValueKind::String) return Value::make_string(args[0].to_string());
                const std::string& raw = args[0].str_val;
                std::ostringstream out;
                out << "'";
                for (std::size_t i = 0; i < raw.size();) {
                    const unsigned char lead = static_cast<unsigned char>(raw[i]);
                    unsigned int cp = lead;
                    std::size_t width = 1;
                    if ((lead & 0xE0) == 0xC0 && i + 1 < raw.size()) { cp = lead & 0x1F; width = 2; }
                    else if ((lead & 0xF0) == 0xE0 && i + 2 < raw.size()) { cp = lead & 0x0F; width = 3; }
                    else if ((lead & 0xF8) == 0xF0 && i + 3 < raw.size()) { cp = lead & 0x07; width = 4; }
                    if (width > 1) {
                        for (std::size_t k = 1; k < width; ++k) cp = (cp << 6) | (static_cast<unsigned char>(raw[i+k]) & 0x3F);
                    }
                    if (cp == '\\' || cp == '\'') out << '\\' << static_cast<char>(cp);
                    else if (cp >= 32 && cp < 127) out << static_cast<char>(cp);
                    else if (cp <= 0xFF) out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << cp << std::dec;
                    else if (cp <= 0xFFFF) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << cp << std::dec;
                    else out << "\\U" << std::hex << std::setw(8) << std::setfill('0') << cp << std::dec;
                    i += width;
                }
                out << "'";
                return Value::make_string(out.str());
            }
            if (fn_name == "breakpoint") {
                if (!arg_content.empty()) return std::nullopt;
                return Value::make_int(0);
            }
            if (fn_name == "Array") {
                auto values = parse_argument_list(arg_content);
                if (values.empty() && !arg_content.empty()) return std::nullopt;
                auto array = Value::make_array(std::move(values));
                if (array.array_val && !array.array_val->empty()) array.array_element_type = array.array_val->front().type_name;
                return array;
            }
            if (fn_name == "array.length" || fn_name == "array.push" || fn_name == "array.pop" ||
                fn_name == "array.set" || fn_name == "array.get" || fn_name == "array.remove" ||
                fn_name == "array.clear") {
                auto args = parse_argument_list(arg_content);
                if (args.empty() || args[0].kind != ValueKind::Array || !args[0].array_val) return std::nullopt;
                auto& items = *args[0].array_val;
                if (fn_name == "array.length") {
                    if (args.size() != 1) return std::nullopt;
                    return Value::make_int(static_cast<long long>(items.size()), "Int32");
                }
                if (fn_name == "array.push") {
                    if (args.size() != 2) return std::nullopt;
                    if (!args[0].array_element_type.empty() && args[0].array_element_type != "Inferred") {
                        if (!value_matches_type(args[1], args[0].array_element_type)) return std::nullopt;
                    } else if (!items.empty() && items.front().type_name != args[1].type_name) {
                        return std::nullopt;
                    }
                    items.push_back(args[1]);
                    return Value::make_bool(true);
                }
                if (fn_name == "array.pop") {
                    if (args.size() != 1 || items.empty()) return std::nullopt;
                    Value removed = std::move(items.back());
                    items.pop_back();
                    return removed;
                }
                if (fn_name == "array.get") {
                    if (args.size() != 2 || args[1].kind != ValueKind::Integer || args[1].int_val < 0 || static_cast<std::size_t>(args[1].int_val) >= items.size()) return std::nullopt;
                    return items[static_cast<std::size_t>(args[1].int_val)];
                }
                if (fn_name == "array.set") {
                    if (args.size() != 3 || args[1].kind != ValueKind::Integer || args[1].int_val < 0 || static_cast<std::size_t>(args[1].int_val) >= items.size()) return std::nullopt;
                    if (!args[0].array_element_type.empty() && args[0].array_element_type != "Inferred") {
                        if (!value_matches_type(args[2], args[0].array_element_type)) return std::nullopt;
                    } else if (!items.empty() && items.front().type_name != args[2].type_name) {
                        return std::nullopt;
                    }
                    items[static_cast<std::size_t>(args[1].int_val)] = args[2];
                    return Value::make_bool(true);
                }
                if (fn_name == "array.remove") {
                    if (args.size() != 2 || args[1].kind != ValueKind::Integer || args[1].int_val < 0 || static_cast<std::size_t>(args[1].int_val) >= items.size()) return std::nullopt;
                    items.erase(items.begin() + args[1].int_val);
                    return Value::make_bool(true);
                }
                if (fn_name == "array.clear") {
                    if (args.size() != 1) return std::nullopt;
                    items.clear();
                    return Value::make_bool(true);
                }
            }
            const auto method_dot = fn_name.find('.');
            if (method_dot != std::string::npos) {
                const std::string object_name = fn_name.substr(0, method_dot);
                const std::string method = fn_name.substr(method_dot + 1);
                auto object = variables_.find(object_name);
                if (object != variables_.end() && object->second.kind == ValueKind::ByteArray && object->second.bytearray_val) {
                    auto& data = *object->second.bytearray_val;
                    auto args = parse_argument_list(arg_content);
                    auto valid_byte = [](const Value& v) { return v.kind == ValueKind::Integer && v.int_val >= 0 && v.int_val <= 255; };
                    if (method == "append") {
                        if (args.empty()) return std::nullopt;
                        for (const auto& v : args) { if (!valid_byte(v)) return std::nullopt; data.push_back(static_cast<unsigned char>(v.int_val)); }
                        return Value::make_bool(true);
                    }
                    if (method == "pop") {
                        if (args.size()!=1 || args[0].kind!=ValueKind::Integer || args[0].int_val<0 || static_cast<std::size_t>(args[0].int_val)>=data.size()) return std::nullopt;
                        const auto index=static_cast<std::size_t>(args[0].int_val); const auto removed=data[index]; data.erase(data.begin()+static_cast<std::ptrdiff_t>(index));
                        return Value::make_int(removed,"UInt8");
                    }
                    if (method == "vpop") {
                        if (args.size()!=1 || !valid_byte(args[0])) return std::nullopt;
                        auto it=std::find(data.begin(),data.end(),static_cast<unsigned char>(args[0].int_val));
                        if (it==data.end()) return std::nullopt;
                        const auto removed=*it; data.erase(it); return Value::make_int(removed,"UInt8");
                    }
                    if (method == "clear") {
                        if (args.empty()) data.clear();
                        else if (args.size()==1 && args[0].kind==ValueKind::Integer && args[0].int_val>=0 && static_cast<std::size_t>(args[0].int_val)<data.size())
                            data.erase(data.begin()+static_cast<std::ptrdiff_t>(args[0].int_val),data.end());
                        else return std::nullopt;
                        return Value::make_bool(true);
                    }
                    if (method == "reverse") { if(!args.empty()) return std::nullopt; std::reverse(data.begin(),data.end()); return Value::make_bool(true); }
                    if (method == "copy") { if(!args.empty()) return std::nullopt; return Value::make_bytearray(data); }
                    if (method == "contains" || method == "index" || method == "count") {
                        if(args.size()!=1 || !valid_byte(args[0])) return std::nullopt;
                        const auto value=static_cast<unsigned char>(args[0].int_val);
                        const auto count=static_cast<long long>(std::count(data.begin(),data.end(),value));
                        if(method=="contains") return Value::make_bool(count>0);
                        if(method=="count") return Value::make_int(count,"Int32");
                        auto it=std::find(data.begin(),data.end(),value); if(it==data.end()) return std::nullopt;
                        return Value::make_int(static_cast<long long>(std::distance(data.begin(),it)),"Int32");
                    }
                    if (method == "extend") {
                        bool at_front=false; std::size_t source_index=0;
                        if(args.size()==2 && args[0].kind==ValueKind::Boolean){at_front=args[0].bool_val;source_index=1;}
                        else if(args.size()!=1) return std::nullopt;
                        std::vector<unsigned char> extra;
                        const auto& source=args[source_index];
                        if(source.kind==ValueKind::Array && source.array_val){for(const auto& v:*source.array_val){if(!valid_byte(v))return std::nullopt;extra.push_back(static_cast<unsigned char>(v.int_val));}}
                        else if(source.kind==ValueKind::ByteArray && source.bytearray_val) extra=*source.bytearray_val;
                        else return std::nullopt;
                        if(at_front) data.insert(data.begin(),extra.begin(),extra.end()); else data.insert(data.end(),extra.begin(),extra.end());
                        return Value::make_bool(true);
                    }
                    if (method == "slice") {
                        if(args.size()!=2 || args[0].kind!=ValueKind::Integer || args[1].kind!=ValueKind::Integer ||
                           args[0].int_val<0 || args[1].int_val<args[0].int_val || static_cast<std::size_t>(args[1].int_val)>data.size()) return std::nullopt;
                        return Value::make_bytearray(std::vector<unsigned char>(data.begin()+args[0].int_val,data.begin()+args[1].int_val));
                    }
                    if (method == "fill") {
                        if(args.size()!=3 || !valid_byte(args[0]) || args[1].kind!=ValueKind::Integer || args[2].kind!=ValueKind::Integer ||
                           args[1].int_val<0 || args[2].int_val<args[1].int_val || static_cast<std::size_t>(args[2].int_val)>data.size()) return std::nullopt;
                        std::fill(data.begin()+args[1].int_val,data.begin()+args[2].int_val,static_cast<unsigned char>(args[0].int_val));
                        return Value::make_bool(true);
                    }
                    if (method == "insert") {
                        // insert[index, value] replaces or applies +, -, * to the existing byte.
                        const auto comma=arg_content.find(',');
                        if(comma==std::string::npos) return std::nullopt;
                        auto index=evaluate_integer_expression(arg_content.substr(0,comma));
                        if(!index || *index<0 || static_cast<std::size_t>(*index)>=data.size()) return std::nullopt;
                        std::string operation=arg_content.substr(comma+1); trim(operation);
                        long long current=data[static_cast<std::size_t>(*index)], result=0;
                        if(operation.size()>1 && (operation[0]=='+' || operation[0]=='-' || operation[0]=='*')){
                            const char op=operation[0];
                            auto operand=evaluate_integer_expression(operation.substr(1));
                            if(!operand) return std::nullopt;
                            long double calculated=static_cast<long double>(current);
                            if(op=='+') calculated+=static_cast<long double>(*operand);
                            else if(op=='-') calculated-=static_cast<long double>(*operand);
                            else calculated*=static_cast<long double>(*operand);
                            if(calculated>255.0L) result=256;
                            else if(calculated<0.0L) result=-1;
                            else result=static_cast<long long>(calculated);
                        } else {
                            auto replacement=evaluate_expression(operation);
                            if(!replacement || !valid_byte(*replacement)) return std::nullopt;
                            result=replacement->int_val;
                        }
                        if(result>255){std::cerr<<"MonC warning: bytearray value clamped to 255.\\n";result=255;}
                        if(result<0){std::cerr<<"MonC warning: bytearray value clamped to 0.\\n";result=0;}
                        data[static_cast<std::size_t>(*index)]=static_cast<unsigned char>(result);
                        return Value::make_bool(true);
                    }
                }
            }
            if (fn_name == "json.parse") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1 || args[0].kind != ValueKind::String || args[0].str_val.size() > 8 * 1024 * 1024) return std::nullopt;
                JsonParser parser(args[0].str_val);
                auto parsed = parser.parse();
                if (!parsed) return std::nullopt;
                return Value::make_json(std::move(parsed));
            }
            if (fn_name == "json.stringify") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1 || args[0].kind != ValueKind::Json) return std::nullopt;
                return Value::make_string(json_serialize(args[0].json_val));
            }
            if (fn_name == "json.type") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1 || args[0].kind != ValueKind::Json) return std::nullopt;
                return Value::make_string(json_type_name(args[0].json_val));
            }
            if (fn_name == "json.length") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1 || args[0].kind != ValueKind::Json || !args[0].json_val) return std::nullopt;
                const auto& node = args[0].json_val;
                if (node->type == JsonNode::Type::Array) return Value::make_int(static_cast<long long>(node->array.size()));
                if (node->type == JsonNode::Type::Object) return Value::make_int(static_cast<long long>(node->object.size()));
                if (node->type == JsonNode::Type::String) return Value::make_int(static_cast<long long>(node->scalar.size()));
                return Value::make_int(0);
            }
            if (fn_name == "json.get") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 2 || args[0].kind != ValueKind::Json || !args[0].json_val) return std::nullopt;
                const auto& node = args[0].json_val;
                if (node->type == JsonNode::Type::Object && args[1].kind == ValueKind::String) {
                    auto found = node->object.find(args[1].str_val);
                    if (found != node->object.end()) return Value::make_json(found->second);
                }
                if (node->type == JsonNode::Type::Array && args[1].kind == ValueKind::Integer &&
                    args[1].int_val >= 0 && static_cast<std::size_t>(args[1].int_val) < node->array.size()) {
                    return Value::make_json(node->array[static_cast<std::size_t>(args[1].int_val)]);
                }
                return std::nullopt;
            }
            if (fn_name == "json.has") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 2 || args[0].kind != ValueKind::Json || !args[0].json_val ||
                    args[0].json_val->type != JsonNode::Type::Object || args[1].kind != ValueKind::String) return std::nullopt;
                return Value::make_bool(args[0].json_val->object.find(args[1].str_val) != args[0].json_val->object.end());
            }
            if (fn_name == "json.new_object" || fn_name == "json.new_array" || fn_name == "json.null") {
                if (!arg_content.empty()) return std::nullopt;
                auto node = std::make_shared<JsonNode>();
                if (fn_name == "json.new_object") node->type = JsonNode::Type::Object;
                else if (fn_name == "json.new_array") node->type = JsonNode::Type::Array;
                return Value::make_json(std::move(node));
            }
            if (fn_name == "json.from_string" || fn_name == "json.from_int" || fn_name == "json.from_bool") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1) return std::nullopt;
                auto node = std::make_shared<JsonNode>();
                if (fn_name == "json.from_string" && args[0].kind == ValueKind::String) {
                    node->type = JsonNode::Type::String; node->scalar = args[0].str_val;
                } else if (fn_name == "json.from_int" && args[0].kind == ValueKind::Integer) {
                    node->type = JsonNode::Type::Number; node->scalar = std::to_string(args[0].int_val);
                } else if (fn_name == "json.from_bool" && args[0].kind == ValueKind::Boolean) {
                    node->type = JsonNode::Type::Boolean; node->boolean_value = args[0].bool_val;
                } else return std::nullopt;
                return Value::make_json(std::move(node));
            }
            if (fn_name == "json.set") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 3 || args[0].kind != ValueKind::Json || !args[0].json_val ||
                    args[0].json_val->type != JsonNode::Type::Object || args[1].kind != ValueKind::String) return std::nullopt;
                auto json_value = value_to_json_node(args[2]);
                if (!json_value) return std::nullopt;
                args[0].json_val->object[args[1].str_val] = std::move(json_value);
                return Value::make_bool(true);
            }
            if (fn_name == "json.push") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 2 || args[0].kind != ValueKind::Json || !args[0].json_val ||
                    args[0].json_val->type != JsonNode::Type::Array) return std::nullopt;
                auto json_value = value_to_json_node(args[1]);
                if (!json_value) return std::nullopt;
                args[0].json_val->array.push_back(std::move(json_value));
                return Value::make_bool(true);
            }
            if (fn_name == "json.read") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 1 || args[0].kind != ValueKind::String) return std::nullopt;
                std::ifstream file(args[0].str_val, std::ios::binary);
                if (!file) return std::nullopt;
                std::ostringstream buffer; buffer << file.rdbuf();
                const std::string contents = buffer.str();
                if (contents.size() > 8 * 1024 * 1024) return std::nullopt;
                JsonParser parser(contents);
                auto parsed = parser.parse();
                if (!parsed) return std::nullopt;
                return Value::make_json(std::move(parsed));
            }
            if (fn_name == "json.write") {
                auto args = parse_argument_list(arg_content);
                if (args.size() != 2 || args[0].kind != ValueKind::String) return std::nullopt;
                auto json_value = value_to_json_node(args[1]);
                if (!json_value) return std::nullopt;
                std::ofstream file(args[0].str_val, std::ios::binary | std::ios::trunc);
                if (!file) return std::nullopt;
                const std::string output = json_serialize(json_value);
                file.write(output.data(), static_cast<std::streamsize>(output.size()));
                if (!file) return std::nullopt;
                return Value::make_bool(true);
            }

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
            if (fn_name == "crypto.key_from") {
                const auto args = parse_argument_list(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (args.empty()) return std::nullopt;
                std::size_t klen = 32;
                if (args.size() >= 2 && args[1].kind == ValueKind::Integer && args[1].int_val > 0) {
                    klen = static_cast<std::size_t>(args[1].int_val);
                }
                auto k = crypto_derive_key_hex(args[0].to_string(), klen);
                if (!k) return std::nullopt;
                return Value::make_string(*k, "SecretKey");
            }
            if (fn_name == "crypto.aes_encrypt") {
                const auto args = parse_argument_list(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (args.size() != 2) return std::nullopt;
                auto ct = aes_gcm_encrypt_hex(args[0].to_string(), args[1].to_string());
                if (!ct) return std::nullopt;
                return Value::make_string(*ct, "Ciphertext");
            }
            if (fn_name == "crypto.aes_decrypt") {
                const auto args = parse_argument_list(text.substr(open_pos + 1, text.size() - open_pos - 2));
                if (args.size() != 2) return std::nullopt;
                auto pt = aes_gcm_decrypt_hex(args[0].to_string(), args[1].to_string());
                if (!pt) return std::nullopt;
                return Value::make_string(*pt, "String");
            }
            const auto variable = variables_.find(fn_name);
            if (variable != variables_.end()) {
                if (moved_variables_.find(fn_name) != moved_variables_.end()) {
                    fail("use of moved value '" + fn_name + "'");
                    return std::nullopt;
                }
                if (variable->second.kind == ValueKind::ByteArray && variable->second.bytearray_val) {
                    const std::string index_text=text.substr(open_pos+1,text.size()-open_pos-2);
                    if(index_text.empty()) return variable->second;
                    auto index=evaluate_integer_expression(index_text);
                    if(!index || *index<0 || static_cast<std::size_t>(*index)>=variable->second.bytearray_val->size()) return std::nullopt;
                    return Value::make_int((*variable->second.bytearray_val)[static_cast<std::size_t>(*index)],"UInt8");
                }
                if (variable->second.kind == ValueKind::Bytes) {
                    auto index = evaluate_integer_expression(text.substr(open_pos + 1, text.size() - open_pos - 2));
                    if (!index || *index < 0 || static_cast<std::size_t>(*index) >= variable->second.bytes_val.size()) return std::nullopt;
                    return Value::make_int(variable->second.bytes_val[static_cast<std::size_t>(*index)], "UInt8");
                }
                if (variable->second.kind == ValueKind::Array) {
                    auto index = evaluate_integer_expression(text.substr(open_pos + 1, text.size() - open_pos - 2));
                    if (!variable->second.array_val || !index || *index < 0 || static_cast<std::size_t>(*index) >= variable->second.array_val->size()) return std::nullopt;
                    return (*variable->second.array_val)[static_cast<std::size_t>(*index)];
                }
            }
            if (functions_.find(fn_name) != functions_.end()) {
                auto args = parse_argument_list(arg_content);
                return call_function(fn_name, args);
            }
        }

        const auto c_open_pos = text.find('(');
        if (c_open_pos != std::string::npos && text.back() == ')') {
            std::string c_name = text.substr(0, c_open_pos);
            trim(c_name);
            if (c_functions_.find(c_name) != c_functions_.end()) {
                const auto args = parse_argument_list(text.substr(c_open_pos + 1, text.size() - c_open_pos - 2));
                return call_c_function(c_name, args);
            }
        }

        if (moved_variables_.find(text) != moved_variables_.end()) {
            fail("use of moved value '" + text + "'");
            return std::nullopt;
        }

        const auto property_dot=text.find('.');
        if(property_dot!=std::string::npos){
            const auto base=text.substr(0,property_dot), property=text.substr(property_dot+1);
            const auto obj=variables_.find(base);
            if(obj!=variables_.end() && obj->second.kind==ValueKind::ByteArray && obj->second.bytearray_val && property=="length")
                return Value::make_int(static_cast<long long>(obj->second.bytearray_val->size()),"Int32");
        }
        auto it = variables_.find(text);
        if (it != variables_.end()) {
            return it->second;
        }

        if (is_float_expression(text)) {
            const auto float_val = evaluate_float_expression(text);
            if (float_val) return Value::make_float(*float_val);
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

            if (left->kind == ValueKind::Float || right->kind == ValueKind::Float) {
                const double left_value = left->kind == ValueKind::Float ? left->float_val : static_cast<double>(left->int_val);
                const double right_value = right->kind == ValueKind::Float ? right->float_val : static_cast<double>(right->int_val);
                if (std::string(op) == "==") return left_value == right_value;
                if (std::string(op) == "!=") return left_value != right_value;
                if (std::string(op) == "<=") return left_value <= right_value;
                if (std::string(op) == ">=") return left_value >= right_value;
                if (std::string(op) == "<") return left_value < right_value;
                return left_value > right_value;
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
        if (moved_variables_.find(text) != moved_variables_.end()) return std::nullopt;
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
            if (v.kind == ValueKind::Float && (type_name == "Float32" || type_name == "Double32")) {
                v.float_val = static_cast<double>(static_cast<float>(v.float_val));
            }
            if (v.kind == ValueKind::Array && type_name.rfind("Array<", 0) == 0 && type_name.back() == '>') {
                v.array_element_type = type_name.substr(6, type_name.size() - 7);
            }
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
        if (position_ < source_.size() && source_[position_] == '[') {
            const auto variable = variables_.find(name);
            if (variable == variables_.end() || variable->second.kind != ValueKind::ByteArray || !variable->second.bytearray_val) {
                position_ = saved_pos;
                return false;
            }
            ++position_;
            const std::size_t index_start = position_;
            int depth = 1;
            while (position_ < source_.size() && depth > 0) {
                if (source_[position_] == '[') ++depth;
                else if (source_[position_] == ']') --depth;
                if (depth > 0) ++position_;
            }
            if (depth != 0) return fail("unterminated bytearray index assignment");
            const std::string index_text = source_.substr(index_start, position_ - index_start);
            ++position_;
            whitespace();
            if (position_ >= source_.size() || source_[position_] != '=') { position_ = saved_pos; return false; }
            ++position_;
            const std::size_t value_start = position_;
            while (position_ < source_.size() && source_[position_] != ';') ++position_;
            if (position_ >= source_.size()) return fail("unterminated bytearray index assignment");
            const std::string value_text = source_.substr(value_start, position_ - value_start);
            ++position_;
            if (active) {
                auto indices = parse_argument_list(index_text);
                auto values = parse_argument_list(value_text);
                if (indices.empty() || indices.size() != values.size()) return fail("bytearray index/value count mismatch");
                std::vector<std::size_t> positions;
                long long previous = -1;
                for (const auto& index : indices) {
                    if (index.kind != ValueKind::Integer || index.int_val < 0 ||
                        static_cast<std::size_t>(index.int_val) >= variable->second.bytearray_val->size() ||
                        index.int_val <= previous) return fail("bytearray indices must be valid, ascending and unique");
                    previous = index.int_val;
                    positions.push_back(static_cast<std::size_t>(index.int_val));
                }
                for (std::size_t i = 0; i < values.size(); ++i) {
                    if (values[i].kind != ValueKind::Integer || values[i].int_val < 0 || values[i].int_val > 255)
                        return fail("bytearray values must be integers between 0 and 255");
                }
                for (std::size_t i = 0; i < positions.size(); ++i)
                    (*variable->second.bytearray_val)[positions[i]] = static_cast<unsigned char>(values[i].int_val);
            }
            return true;
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
            if (v.kind == ValueKind::Float &&
                (v.type_name == "Float32" || v.type_name == "Double32")) {
                v.float_val = static_cast<double>(static_cast<float>(v.float_val));
            }
            if (v.kind == ValueKind::Array) v.array_element_type = it->second.array_element_type;
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
            iteration.c_functions_ = c_functions_;
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
            iteration.c_functions_ = c_functions_;
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
    std::unordered_set<std::string> moved_variables_;
    std::unordered_set<std::string> constants_;
    std::unordered_map<std::string, FunctionDef> functions_;
    std::unordered_map<std::string, CFunctionBinding> c_functions_;
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
        std::cerr << "monc: error: cannot read source file: " << source_path.string() << '\n';
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
        if (!input) { std::cerr << "monc: error: cannot read source file\n"; return 1; }
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
        if (!input) { std::cerr << "monc: error: cannot read source file\n"; return 1; }
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

    // Preferred mode: monc run path/to/main.monc
    if (argc == 3 && std::string(argv[1]) == "run") {
        const std::filesystem::path source_path = argv[2];
        if (source_path.extension() == ".monc") {
            if (!std::filesystem::is_regular_file(source_path)) {
                std::cerr << "monc: error: source file not found: " << source_path.string() << '\n';
                return 1;
            }
            const int result = execute_print_subset(source_path);
            return result < 0 ? 1 : result;
        }
    }

    // Short direct mode remains available: monc program.monc
    if (argc == 2) {
        const std::filesystem::path source_path = argv[1];
        if (source_path.extension() == ".monc") {
            if (!std::filesystem::is_regular_file(source_path)) {
                std::cerr << "monc: error: source file not found: " << source_path.string() << '\n';
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
        std::cerr << "monc: error: " << resolution.error << '\n';
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
