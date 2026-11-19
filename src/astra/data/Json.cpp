#include "astra/data/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace astra::json {
namespace {

constexpr int kMaxDepth = 200;

class Parser {
public:
    Parser(std::string_view text, std::string* error) : text_(text), error_(error) {}

    bool Run(Value& out) {
        SkipWhitespace();
        if (!ParseValue(out, 0)) return false;
        SkipWhitespace();
        if (pos_ != text_.size()) return Fail("trailing characters after document");
        return true;
    }

private:
    bool Fail(const char* message) {
        if (error_ && error_->empty()) {
            *error_ = std::string(message) + " at offset " + std::to_string(pos_);
        }
        return false;
    }

    char Peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    void SkipWhitespace() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool Consume(char expected) {
        if (Peek() != expected) return Fail("unexpected character");
        ++pos_;
        return true;
    }

    bool ParseValue(Value& out, int depth) {
        if (depth > kMaxDepth) return Fail("document nested too deeply");
        if (pos_ >= text_.size()) return Fail("unexpected end of input");
        switch (Peek()) {
            case '{': return ParseObject(out, depth);
            case '[': return ParseArray(out, depth);
            case '"': {
                std::string s;
                if (!ParseString(s)) return false;
                out = Value(std::move(s));
                return true;
            }
            case 't':
                if (text_.substr(pos_, 4) == "true") {
                    pos_ += 4;
                    out = Value(true);
                    return true;
                }
                return Fail("invalid literal");
            case 'f':
                if (text_.substr(pos_, 5) == "false") {
                    pos_ += 5;
                    out = Value(false);
                    return true;
                }
                return Fail("invalid literal");
            case 'n':
                if (text_.substr(pos_, 4) == "null") {
                    pos_ += 4;
                    out = Value();
                    return true;
                }
                return Fail("invalid literal");
            default: return ParseNumber(out);
        }
    }

    bool ParseNumber(Value& out) {
        const std::size_t start = pos_;
        if (Peek() == '-') ++pos_;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' ||
                c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        if (start == pos_) return Fail("invalid number");
        const std::string token(text_.substr(start, pos_ - start));
        char* end = nullptr;
        const double value = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0') return Fail("invalid number");
        out = Value(value);
        return true;
    }

    static void AppendUtf8(std::string& out, unsigned codePoint) {
        if (codePoint < 0x80) {
            out += static_cast<char>(codePoint);
        } else if (codePoint < 0x800) {
            out += static_cast<char>(0xC0 | (codePoint >> 6));
            out += static_cast<char>(0x80 | (codePoint & 0x3F));
        } else if (codePoint < 0x10000) {
            out += static_cast<char>(0xE0 | (codePoint >> 12));
            out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codePoint & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (codePoint >> 18));
            out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }

    bool ParseHex4(unsigned& value) {
        if (pos_ + 4 > text_.size()) return Fail("truncated unicode escape");
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else return Fail("invalid unicode escape");
        }
        return true;
    }

    bool ParseString(std::string& out) {
        if (!Consume('"')) return false;
        out.clear();
        while (true) {
            if (pos_ >= text_.size()) return Fail("unterminated string");
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) return Fail("unterminated escape");
            const char esc = text_[pos_++];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned codePoint = 0;
                    if (!ParseHex4(codePoint)) return false;
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF && pos_ + 6 <= text_.size() &&
                        text_[pos_] == '\\' && text_[pos_ + 1] == 'u') {
                        pos_ += 2;
                        unsigned low = 0;
                        if (!ParseHex4(low)) return false;
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    AppendUtf8(out, codePoint);
                    break;
                }
                default: return Fail("invalid escape sequence");
            }
        }
    }

    bool ParseObject(Value& out, int depth) {
        if (!Consume('{')) return false;
        out = Value::MakeObject();
        SkipWhitespace();
        if (Peek() == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipWhitespace();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWhitespace();
            if (!Consume(':')) return false;
            SkipWhitespace();
            Value value;
            if (!ParseValue(value, depth + 1)) return false;
            out.Set(std::move(key), std::move(value));
            SkipWhitespace();
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == '}') {
                ++pos_;
                return true;
            }
            return Fail("expected ',' or '}'");
        }
    }

    bool ParseArray(Value& out, int depth) {
        if (!Consume('[')) return false;
        out = Value::MakeArray();
        SkipWhitespace();
        if (Peek() == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipWhitespace();
            Value value;
            if (!ParseValue(value, depth + 1)) return false;
            out.Push(std::move(value));
            SkipWhitespace();
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == ']') {
                ++pos_;
                return true;
            }
            return Fail("expected ',' or ']'");
        }
    }

    std::string_view text_;
    std::string* error_;
    std::size_t pos_ = 0;
};

void EscapeTo(std::string& out, std::string_view s) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

void Indent(std::string& out, int level) { out.append(static_cast<std::size_t>(level) * 2, ' '); }

}  // namespace

const Value& Null() {
    static const Value kNull;
    return kNull;
}

Value Value::MakeArray() {
    Value v;
    v.type_ = Type::Array;
    return v;
}

Value Value::MakeObject() {
    Value v;
    v.type_ = Type::Object;
    return v;
}

bool Value::AsBool(bool fallback) const { return type_ == Type::Bool ? bool_ : fallback; }

double Value::AsNumber(double fallback) const { return type_ == Type::Number ? number_ : fallback; }

long long Value::AsInt(long long fallback) const {
    return type_ == Type::Number ? static_cast<long long>(number_) : fallback;
}

std::string Value::AsString(std::string_view fallback) const {
    return type_ == Type::String ? string_ : std::string(fallback);
}

bool Value::Has(std::string_view key) const {
    return type_ == Type::Object && object_.find(std::string(key)) != object_.end();
}

const Value& Value::operator[](std::string_view key) const {
    if (type_ != Type::Object) return Null();
    const auto it = object_.find(std::string(key));
    return it == object_.end() ? Null() : it->second;
}

Value& Value::operator[](std::string_view key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    return object_[std::string(key)];
}

void Value::Set(std::string key, Value value) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    object_[std::move(key)] = std::move(value);
}

void Value::Erase(std::string_view key) {
    if (type_ == Type::Object) object_.erase(std::string(key));
}

std::size_t Value::Size() const {
    if (type_ == Type::Array) return array_.size();
    if (type_ == Type::Object) return object_.size();
    return 0;
}

const Value& Value::At(std::size_t index) const {
    if (type_ != Type::Array || index >= array_.size()) return Null();
    return array_[index];
}

void Value::Push(Value value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
    }
    array_.push_back(std::move(value));
}

std::string Value::Dump(bool pretty) const {
    std::string out;
    DumpTo(out, pretty, 0);
    return out;
}

void Value::DumpTo(std::string& out, bool pretty, int level) const {
    switch (type_) {
        case Type::Null: out += "null"; return;
        case Type::Bool: out += bool_ ? "true" : "false"; return;
        case Type::Number: {
            if (std::isfinite(number_) && number_ == static_cast<double>(static_cast<long long>(number_)) &&
                std::fabs(number_) < 9.0e15) {
                out += std::to_string(static_cast<long long>(number_));
            } else {
                char buf[40];
                std::snprintf(buf, sizeof(buf), "%.17g", number_);
                out += buf;
            }
            return;
        }
        case Type::String: EscapeTo(out, string_); return;
        case Type::Array: {
            if (array_.empty()) {
                out += "[]";
                return;
            }
            out += '[';
            for (std::size_t i = 0; i < array_.size(); ++i) {
                if (i) out += ',';
                if (pretty) {
                    out += '\n';
                    Indent(out, level + 1);
                }
                array_[i].DumpTo(out, pretty, level + 1);
            }
            if (pretty) {
                out += '\n';
                Indent(out, level);
            }
            out += ']';
            return;
        }
        case Type::Object: {
            if (object_.empty()) {
                out += "{}";
                return;
            }
            out += '{';
            bool first = true;
            for (const auto& [key, value] : object_) {
                if (!first) out += ',';
                first = false;
                if (pretty) {
                    out += '\n';
                    Indent(out, level + 1);
                }
                EscapeTo(out, key);
                out += pretty ? ": " : ":";
                value.DumpTo(out, pretty, level + 1);
            }
            if (pretty) {
                out += '\n';
                Indent(out, level);
            }
            out += '}';
            return;
        }
    }
}

bool Value::Parse(std::string_view text, Value& out, std::string* error) {
    if (error) error->clear();
    Parser parser(text, error);
    Value parsed;
    if (!parser.Run(parsed)) return false;
    out = std::move(parsed);
    return true;
}

Value Value::ParseOrNull(std::string_view text, std::string* error) {
    Value value;
    if (!Parse(text, value, error)) return Value();
    return value;
}

}  // namespace astra::json
