// script/Interpreter.h - tree-walking evaluator for AstraScript.
//
// The interpreter owns the global scope and a stack of local scopes. Hosts (the editor, the
// self-check) inject their APIs as natives before a run, so the language never needs to know
// what a scene or an image is.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "astra/script/Script.h"
#include "astra/script/Value.h"

namespace astra::script {

struct RunResult {
    bool ok = false;
    std::string error;
    int line = 0;
    int column = 0;
    long long steps = 0;
    double milliseconds = 0.0;
    Value returned;
};

class Interpreter {
public:
    using PrintSink = std::function<void(const std::string&)>;

    Interpreter();
    ~Interpreter();

    void Define(const std::string& name, Value value);
    bool Defined(const std::string& name) const;
    void SetPrintSink(PrintSink sink);
    void Print(const std::string& line);

    // Runs a parsed program at global scope. The program must outlive the run.
    RunResult Run(const Program& program);

    // Calls any callable value (used by hosts to invoke script entry points).
    Value Call(const Value& callable, std::vector<Value> arguments);

    long long steps() const { return steps_; }
    void SetStepLimit(long long limit) { stepLimit_ = limit; }

    // Thrown for every runtime error; hosts catch it and turn it into a diagnostic.
    struct Error {
        std::string message;
        int line = 1;
        int column = 1;
    };

private:
    using ScriptError = Error;

    struct Binding {
        Value value;
        bool mutableBinding = true;
    };
    using Scope = std::unordered_map<std::string, Binding>;

    enum class Flow { Normal, Break, Continue, Return };

    Value Evaluate(const Expr& expr);
    Flow Execute(const Stmt& statement, Value& returnValue);
    Flow ExecuteBlock(const std::vector<StmtPtr>& body, Value& returnValue);

    Binding* Lookup(const std::string& name);
    void Declare(const std::string& name, Value value, bool mutableBinding);
    void Tick(int line);

    bool Equals(const Value& a, const Value& b) const;

    std::vector<Scope> scopes_;
    PrintSink print_;
    long long steps_ = 0;
    long long stepLimit_ = 4000000;
};

}  // namespace astra::script
