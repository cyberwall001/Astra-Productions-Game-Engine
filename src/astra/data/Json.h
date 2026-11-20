// data/Json.h - minimal but strict JSON value, parser and writer.
//
// Objects keep their keys sorted so serialized files diff cleanly between runs.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace astra::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool v) : type_(Type::Bool), bool_(v) {}
    Value(double v) : type_(Type::Number), number_(v) {}
    Value(int v) : type_(Type::Number), number_(static_cast<double>(v)) {}
    Value(long long v) : type_(Type::Number), number_(static_cast<double>(v)) {}
    Value(long v) : type_(Type::Number), number_(static_cast<double>(v)) {}
    Value(std::string v) : type_(Type::String), string_(std::move(v)) {}
    Value(const char* v) : type_(Type::String), string_(v ? v : "") {}

    static Value MakeArray();
    static Value MakeObject();

    Type GetType() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }
    bool IsBool() const { return type_ == Type::Bool; }
    bool IsNumber() const { return type_ == Type::Number; }
    bool IsString() const { return type_ == Type::String; }
    bool IsArray() const { return type_ == Type::Array; }
    bool IsObject() const { return type_ == Type::Object; }

    // Scalar accessors with defaults.
    bool AsBool(bool fallback = false) const;
    double AsNumber(double fallback = 0.0) const;
    long long AsInt(long long fallback = 0) const;
    std::string AsString(std::string_view fallback = "") const;

    // Object access. Missing keys return a shared null value.
    bool Has(std::string_view key) const;
    const Value& operator[](std::string_view key) const;
    Value& operator[](std::string_view key);  // inserts null when missing
    void Set(std::string key, Value value);
    void Erase(std::string_view key);
    const std::map<std::string, Value>& Items() const { return object_; }

    // Array access.
    std::size_t Size() const;
    const Value& At(std::size_t index) const;
    void Push(Value value);
    const std::vector<Value>& Elements() const { return array_; }

    std::string Dump(bool pretty = true) const;
    void DumpTo(std::string& out, bool pretty, int indent) const;

    static bool Parse(std::string_view text, Value& out, std::string* error = nullptr);
    static Value ParseOrNull(std::string_view text, std::string* error = nullptr);

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::map<std::string, Value> object_;
};

const Value& Null();

}  // namespace astra::json
