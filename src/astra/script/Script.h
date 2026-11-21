// script/Script.h - AstraScript: a small C-like scripting language for the editor.
//
//   fn wave(n) { let m = mesh.sphere(32, 20); ... return m; }
//
// The pipeline is the classic three stages so the editor can report real diagnostics:
//   Lexer -> Parser (AST) -> Interpreter (tree walking, step budgeted)
//
// Host functions (scene, mesh, image, math, print) are injected by script::Host, so the
// language core stays engine-agnostic and unit-testable on its own.
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "astra/script/Value.h"

namespace astra::script {

enum class TokenType {
    End,
    Number,
    String,
    Identifier,
    // punctuation
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Dot, Semicolon, Colon,
    // operators
    Plus, Minus, Star, Slash, Percent,
    Bang, NotEqual, Assign,
    EqualEqual, Less, LessEqual, Greater, GreaterEqual,
    AndAnd, OrOr,
    // keywords
    Let, Var, Fn, Return, If, Else, While, For, In, Break, Continue, True, False, Nil,
};

struct Token {
    TokenType type = TokenType::End;
    std::string text;
    double number = 0.0;
    int line = 1;
    int column = 1;
};

struct Diagnostic {
    std::string message;
    int line = 1;
    int column = 1;
};

// ---------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------
struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

enum class ExprKind {
    Literal,
    Variable,
    Unary,
    Binary,
    Logical,
    Call,
    Index,
    ArrayLiteral,
    Member,
};

struct Expr {
    ExprKind kind = ExprKind::Literal;
    int line = 1;
    Value literal;              // Literal
    std::string name;           // Variable / Member name
    TokenType op = TokenType::End;  // Unary / Binary / Logical
    ExprPtr left;
    ExprPtr right;
    ExprPtr target;             // Call callee / Index target / Member object
    std::vector<ExprPtr> arguments;
    std::vector<ExprPtr> elements;
};

enum class StmtKind {
    Let,
    Assign,
    Expression,
    If,
    While,
    ForIn,
    Function,
    Return,
    Break,
    Continue,
    Block,
};

struct Stmt {
    StmtKind kind = StmtKind::Expression;
    int line = 1;
    std::string name;            // Let / Function / ForIn variable
    bool mutableBinding = true;  // let (immutable) vs var
    ExprPtr expression;          // Let value, Expression, Return, Assign value, If/While cond
    ExprPtr assignTarget;        // Assign target (variable or index)
    std::vector<StmtPtr> body;
    std::vector<StmtPtr> elseBody;
    std::vector<std::string> params;  // Function
    bool hasForVariable = false;
};

struct Program {
    std::vector<StmtPtr> statements;

    // Parses source. Returns an empty program and fills `diagnostic` on failure.
    static Program Parse(std::string_view source, Diagnostic* diagnostic);
};

// Tokenises source - exposed so the script editor can colour code without parsing.
std::vector<Token> Tokenize(std::string_view source, Diagnostic* diagnostic);

}  // namespace astra::script
