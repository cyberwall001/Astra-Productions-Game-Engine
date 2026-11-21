#include "astra/script/Interpreter.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace astra::script {
namespace {

void RequireNumbers(const Value& left, const Value& right, const char* op, int line) {
    if (left.IsNumber() && right.IsNumber()) return;
    throw Interpreter::Error{std::string("'") + op + "' needs two numbers, got " +
                                 left.TypeName() + " and " + right.TypeName(),
                             line, 1};
}

}  // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
Interpreter::Interpreter() { scopes_.emplace_back(); }
Interpreter::~Interpreter() = default;

void Interpreter::Define(const std::string& name, Value value) {
    scopes_.front()[name] = Binding{std::move(value), true};
}

bool Interpreter::Defined(const std::string& name) const {
    return scopes_.front().count(name) > 0;
}

void Interpreter::SetPrintSink(PrintSink sink) { print_ = std::move(sink); }

void Interpreter::Print(const std::string& line) {
    if (print_) print_(line);
}

void Interpreter::Tick(int line) {
    ++steps_;
    if (steps_ > stepLimit_) {
        throw ScriptError{"script exceeded its instruction budget (infinite loop?)", line, 1};
    }
}

Interpreter::Binding* Interpreter::Lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

void Interpreter::Declare(const std::string& name, Value value, bool mutableBinding) {
    scopes_.back()[name] = Binding{std::move(value), mutableBinding};
}

bool Interpreter::Equals(const Value& a, const Value& b) const {
    if (a.type != b.type) {
        // Numbers and bools compare across the boundary so `if (x == true)` is not a trap.
        if (a.IsNumber() && b.IsBool()) return (a.number != 0.0) == b.boolValue;
        if (a.IsBool() && b.IsNumber()) return a.boolValue == (b.number != 0.0);
        return false;
    }
    switch (a.type) {
        case Value::Type::Nil: return true;
        case Value::Type::Bool: return a.boolValue == b.boolValue;
        case Value::Type::Number: return a.number == b.number;
        case Value::Type::String: return a.string == b.string;
        case Value::Type::Array: return a.array == b.array;
        case Value::Type::Function: return a.function == b.function;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
Value Interpreter::Evaluate(const Expr& expr) {
    Tick(expr.line);
    switch (expr.kind) {
        case ExprKind::Literal:
            return expr.literal;

        case ExprKind::Variable: {
            Binding* binding = Lookup(expr.name);
            if (!binding) {
                throw ScriptError{"unknown identifier '" + expr.name + "'", expr.line, 1};
            }
            return binding->value;
        }

        case ExprKind::ArrayLiteral: {
            Value array = Value::MakeArray();
            for (const auto& element : expr.elements) {
                array.array->items.push_back(Evaluate(*element));
            }
            return array;
        }

        case ExprKind::Unary: {
            const Value value = Evaluate(*expr.right);
            if (expr.op == TokenType::Bang) return Value(!value.Truthy());
            if (!value.IsNumber()) {
                throw ScriptError{"unary '-' needs a number, got " + value.TypeName(), expr.line, 1};
            }
            return Value(-value.number);
        }

        case ExprKind::Logical: {
            const Value left = Evaluate(*expr.left);
            if (expr.op == TokenType::AndAnd) {
                if (!left.Truthy()) return left;
                return Evaluate(*expr.right);
            }
            if (left.Truthy()) return left;
            return Evaluate(*expr.right);
        }

        case ExprKind::Binary: {
            const Value left = Evaluate(*expr.left);
            const Value right = Evaluate(*expr.right);
            switch (expr.op) {
                case TokenType::Plus: {
                    if (left.IsNumber() && right.IsNumber()) return Value(left.number + right.number);
                    if (left.IsString() || right.IsString()) {
                        return Value(left.ToString() + right.ToString());
                    }
                    throw ScriptError{"'+' needs numbers or strings, got " + left.TypeName() +
                                          " and " + right.TypeName(),
                                      expr.line, 1};
                }
                case TokenType::Minus:
                    RequireNumbers(left, right, "-", expr.line);
                    return Value(left.number - right.number);
                case TokenType::Star:
                    RequireNumbers(left, right, "*", expr.line);
                    return Value(left.number * right.number);
                case TokenType::Slash:
                    RequireNumbers(left, right, "/", expr.line);
                    if (right.number == 0.0) {
                        throw ScriptError{"division by zero", expr.line, 1};
                    }
                    return Value(left.number / right.number);
                case TokenType::Percent:
                    RequireNumbers(left, right, "%", expr.line);
                    if (right.number == 0.0) {
                        throw ScriptError{"modulo by zero", expr.line, 1};
                    }
                    return Value(std::fmod(left.number, right.number));
                case TokenType::EqualEqual: return Value(Equals(left, right));
                case TokenType::NotEqual: return Value(!Equals(left, right));
                case TokenType::Less:
                case TokenType::LessEqual:
                case TokenType::Greater:
                case TokenType::GreaterEqual: {
                    int comparison = 0;
                    if (left.IsNumber() && right.IsNumber()) {
                        comparison = left.number < right.number ? -1
                                     : (left.number > right.number ? 1 : 0);
                    } else if (left.IsString() && right.IsString()) {
                        comparison = left.string < right.string ? -1
                                     : (left.string > right.string ? 1 : 0);
                    } else {
                        throw ScriptError{"comparison needs two numbers or two strings", expr.line,
                                          1};
                    }
                    switch (expr.op) {
                        case TokenType::Less: return Value(comparison < 0);
                        case TokenType::LessEqual: return Value(comparison <= 0);
                        case TokenType::Greater: return Value(comparison > 0);
                        default: return Value(comparison >= 0);
                    }
                }
                default:
                    throw ScriptError{"unsupported operator", expr.line, 1};
            }
        }

        case ExprKind::Index: {
            const Value target = Evaluate(*expr.target);
            const Value index = Evaluate(*expr.right);
            if (target.IsArray()) {
                if (!index.IsNumber()) {
                    throw ScriptError{"array index must be a number", expr.line, 1};
                }
                const int i = static_cast<int>(index.number);
                if (i < 0 || i >= static_cast<int>(target.array->items.size())) {
                    return Value(nullptr);  // out of range reads are nil, not fatal
                }
                return target.array->items[static_cast<std::size_t>(i)];
            }
            if (target.IsString()) {
                if (!index.IsNumber()) {
                    throw ScriptError{"string index must be a number", expr.line, 1};
                }
                const int i = static_cast<int>(index.number);
                if (i < 0 || i >= static_cast<int>(target.string.size())) return Value("");
                return Value(std::string(1, target.string[static_cast<std::size_t>(i)]));
            }
            throw ScriptError{"cannot index " + target.TypeName(), expr.line, 1};
        }

        case ExprKind::Member: {
            const Value target = Evaluate(*expr.target);
            if (expr.name == "length" || expr.name == "count" || expr.name == "size") {
                if (target.IsArray()) {
                    return Value(static_cast<double>(target.array->items.size()));
                }
                if (target.IsString()) return Value(static_cast<double>(target.string.size()));
                return Value(0.0);
            }
            throw ScriptError{"'" + expr.name + "' is not a property of " + target.TypeName(),
                              expr.line, 1};
        }

        case ExprKind::Call: {
            if (expr.target->kind == ExprKind::Member) {
                const std::string& method = expr.target->name;
                const Value receiver = Evaluate(*expr.target->target);
                if (receiver.IsArray() && method == "push") {
                    std::vector<Value> arguments;
                    for (const auto& argument : expr.arguments) {
                        arguments.push_back(Evaluate(*argument));
                    }
                    for (auto& argument : arguments) {
                        receiver.array->items.push_back(std::move(argument));
                    }
                    return receiver;
                }
                if (receiver.IsArray() && method == "pop") {
                    if (receiver.array->items.empty()) return Value(nullptr);
                    Value last = receiver.array->items.back();
                    receiver.array->items.pop_back();
                    return last;
                }
                if (receiver.IsArray() && method == "clear") {
                    receiver.array->items.clear();
                    return receiver;
                }
                throw ScriptError{"'" + method + "' is not a method of " + receiver.TypeName(),
                                  expr.line, 1};
            }

            const Value callee = Evaluate(*expr.target);
            std::vector<Value> arguments;
            arguments.reserve(expr.arguments.size());
            for (const auto& argument : expr.arguments) {
                arguments.push_back(Evaluate(*argument));
            }
            if (!callee.IsFunction()) {
                throw ScriptError{"'" + expr.target->name + "' is not callable", expr.line, 1};
            }
            try {
                return Call(callee, std::move(arguments));
            } catch (ScriptError& error) {
                if (error.line == 1) error.line = expr.line;
                throw;
            }
        }
    }
    throw ScriptError{"unsupported expression", expr.line, 1};
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------
Interpreter::Flow Interpreter::ExecuteBlock(const std::vector<StmtPtr>& body, Value& returnValue) {
    scopes_.emplace_back();
    Flow flow = Flow::Normal;
    try {
        for (const auto& statement : body) {
            flow = Execute(*statement, returnValue);
            if (flow != Flow::Normal) break;
        }
    } catch (...) {
        scopes_.pop_back();
        throw;
    }
    scopes_.pop_back();
    return flow;
}

Interpreter::Flow Interpreter::Execute(const Stmt& statement, Value& returnValue) {
    Tick(statement.line);
    switch (statement.kind) {
        case StmtKind::Let: {
            Value value;
            if (statement.expression) value = Evaluate(*statement.expression);
            // `let` and `var` both create mutable locals; functions stay immutable so a
            // script cannot accidentally overwrite an API entry point.
            Declare(statement.name, std::move(value), true);
            return Flow::Normal;
        }

        case StmtKind::Assign: {
            const Expr& target = *statement.assignTarget;
            Value value = statement.expression ? Evaluate(*statement.expression) : Value(nullptr);
            if (target.kind == ExprKind::Variable) {
                Binding* binding = Lookup(target.name);
                if (!binding) {
                    throw ScriptError{"unknown identifier '" + target.name + "'", statement.line, 1};
                }
                if (!binding->mutableBinding) {
                    throw ScriptError{"'" + target.name + "' is a constant (use var)", statement.line,
                                      1};
                }
                binding->value = std::move(value);
                return Flow::Normal;
            }
            if (target.kind == ExprKind::Index) {
                Value container = Evaluate(*target.target);
                Value index = Evaluate(*target.right);
                if (!container.IsArray() || !index.IsNumber()) {
                    throw ScriptError{"indexed assignment needs an array and a number index",
                                      statement.line, 1};
                }
                const int i = static_cast<int>(index.number);
                if (i < 0 || i >= static_cast<int>(container.array->items.size())) {
                    throw ScriptError{"array index out of range", statement.line, 1};
                }
                container.array->items[static_cast<std::size_t>(i)] = std::move(value);
                return Flow::Normal;
            }
            throw ScriptError{"invalid assignment target", statement.line, 1};
        }

        case StmtKind::Expression:
            if (statement.expression) Evaluate(*statement.expression);
            return Flow::Normal;

        case StmtKind::If: {
            if (statement.expression && Evaluate(*statement.expression).Truthy()) {
                return ExecuteBlock(statement.body, returnValue);
            }
            if (!statement.elseBody.empty()) {
                return ExecuteBlock(statement.elseBody, returnValue);
            }
            return Flow::Normal;
        }

        case StmtKind::While: {
            int guard = 0;
            while (statement.expression && Evaluate(*statement.expression).Truthy()) {
                const Flow flow = ExecuteBlock(statement.body, returnValue);
                if (flow == Flow::Break) break;
                if (flow == Flow::Return) return flow;
                if (++guard > 1000000) {
                    throw ScriptError{"loop ran too long", statement.line, 1};
                }
            }
            return Flow::Normal;
        }

        case StmtKind::ForIn: {
            const Value iterable = statement.expression ? Evaluate(*statement.expression) : Value();
            std::vector<Value> items;
            if (iterable.IsArray()) {
                items = iterable.array->items;
            } else if (iterable.IsString()) {
                for (char c : iterable.string) items.push_back(Value(std::string(1, c)));
            } else if (iterable.IsNumber()) {
                const int count = static_cast<int>(iterable.number);
                for (int i = 0; i < count; ++i) items.push_back(Value(static_cast<double>(i)));
            } else {
                throw ScriptError{"cannot iterate over " + iterable.TypeName(), statement.line, 1};
            }
            for (const auto& item : items) {
                scopes_.emplace_back();
                scopes_.back()[statement.name] = Binding{item, true};
                const Flow flow = ExecuteBlock(statement.body, returnValue);
                scopes_.pop_back();
                if (flow == Flow::Break) break;
                if (flow == Flow::Return) return flow;
            }
            return Flow::Normal;
        }

        case StmtKind::Function: {
            auto function = std::make_shared<FunctionObj>();
            function->name = statement.name.empty() ? "<anonymous>" : statement.name;
            function->params = statement.params;
            function->body = &statement;
            Declare(statement.name, Value(std::move(function)), false);
            return Flow::Normal;
        }

        case StmtKind::Return:
            returnValue = statement.expression ? Evaluate(*statement.expression) : Value(nullptr);
            return Flow::Return;

        case StmtKind::Break:
            return Flow::Break;

        case StmtKind::Continue:
            return Flow::Continue;

        case StmtKind::Block:
            return ExecuteBlock(statement.body, returnValue);
    }
    return Flow::Normal;
}

// ---------------------------------------------------------------------------
// Calls
// ---------------------------------------------------------------------------
Value Interpreter::Call(const Value& callable, std::vector<Value> arguments) {
    if (!callable.IsFunction()) {
        throw ScriptError{"value is not callable", 1, 1};
    }
    const FunctionObj& function = *callable.function;
    if (function.isNative) {
        try {
            return function.native(*this, arguments);
        } catch (ScriptError&) {
            throw;
        } catch (const std::exception& error) {
            throw ScriptError{std::string("native error: ") + error.what(), 1, 1};
        }
    }
    if (!function.body) return Value(nullptr);

    Scope scope;
    for (std::size_t i = 0; i < function.params.size(); ++i) {
        Value value = i < arguments.size() ? std::move(arguments[i]) : Value(nullptr);
        scope[function.params[i]] = Binding{std::move(value), true};
    }
    scopes_.push_back(std::move(scope));
    Value returnValue;
    Flow flow = Flow::Normal;
    try {
        for (const auto& statement : function.body->body) {
            flow = Execute(*statement, returnValue);
            if (flow == Flow::Return) break;
        }
    } catch (...) {
        scopes_.pop_back();
        throw;
    }
    scopes_.pop_back();
    (void)flow;
    return returnValue;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
RunResult Interpreter::Run(const Program& program) {
    const auto start = std::chrono::steady_clock::now();
    RunResult result;
    steps_ = 0;
    Value returnValue;
    try {
        // Global statements execute in the existing global scope.
        for (const auto& statement : program.statements) {
            const Flow flow = Execute(*statement, returnValue);
            if (flow == Flow::Return) break;
        }
        result.ok = true;
        result.returned = returnValue;
    } catch (const ScriptError& error) {
        result.ok = false;
        result.error = error.message;
        result.line = error.line;
        result.column = error.column;
    } catch (const std::exception& error) {
        result.ok = false;
        result.error = std::string("internal error: ") + error.what();
    }
    const auto end = std::chrono::steady_clock::now();
    result.steps = steps_;
    result.milliseconds =
        std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

}  // namespace astra::script
