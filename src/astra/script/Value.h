// script/Value.h - the dynamic value model AstraScript runs on.
//
// Deliberately small: nil, bool, number, string, array and function. Anything heavier
// (meshes, images, actors) is handed to scripts as an integer handle into a host-side table,
// which keeps the interpreter free of engine types and makes the value type cheap to copy.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace astra::script {

class Interpreter;
struct Stmt;
struct Value;

struct ArrayObj {
    std::vector<Value> items;
};

using NativeFunction = std::function<Value(Interpreter&, const std::vector<Value>&)>;

struct FunctionObj {
    std::string name;
    std::vector<std::string> params;
    const Stmt* body = nullptr;  // owned by the parsed program
    std::vector<std::string> capturedScopes;  // unused for now; kept for forward compat
    NativeFunction native;
    bool isNative = false;
};

struct Value {
    enum class Type { Nil, Bool, Number, String, Array, Function };

    Type type = Type::Nil;
    bool boolValue = false;
    double number = 0.0;
    std::string string;
    std::shared_ptr<ArrayObj> array;
    std::shared_ptr<FunctionObj> function;

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool v) : type(Type::Bool), boolValue(v) {}
    Value(double v) : type(Type::Number), number(v) {}
    Value(int v) : type(Type::Number), number(static_cast<double>(v)) {}
    Value(float v) : type(Type::Number), number(static_cast<double>(v)) {}
    Value(long long v) : type(Type::Number), number(static_cast<double>(v)) {}
    Value(const char* v) : type(Type::String), string(v ? v : "") {}
    Value(std::string v) : type(Type::String), string(std::move(v)) {}
    Value(std::shared_ptr<ArrayObj> v) : type(Type::Array), array(std::move(v)) {}
    Value(std::shared_ptr<FunctionObj> v) : type(Type::Function), function(std::move(v)) {}

    bool IsNil() const { return type == Type::Nil; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsArray() const { return type == Type::Array; }
    bool IsFunction() const { return type == Type::Function; }

    bool Truthy() const;
    std::string ToString() const;
    std::string TypeName() const;

    static Value MakeArray() {
        auto object = std::make_shared<ArrayObj>();
        return Value(std::move(object));
    }
    static Value MakeNative(std::string name, NativeFunction fn) {
        auto object = std::make_shared<FunctionObj>();
        object->name = std::move(name);
        object->native = std::move(fn);
        object->isNative = true;
        return Value(std::move(object));
    }
};

}  // namespace astra::script
