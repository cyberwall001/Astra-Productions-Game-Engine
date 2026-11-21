#include "astra/script/Script.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace astra::script {

bool Value::Truthy() const {
    switch (type) {
        case Type::Nil:
            return false;
        case Type::Bool:
            return boolValue;
        case Type::Number:
            return number != 0.0 && number == number;  // NaN is falsy
        case Type::String:
            return !string.empty();
        case Type::Array:
            return array && !array->items.empty();
        case Type::Function:
            return function != nullptr;
    }
    return false;
}

std::string Value::ToString() const {
    switch (type) {
        case Type::Nil:
            return "nil";
        case Type::Bool:
            return boolValue ? "true" : "false";
        case Type::Number: {
            // Integers print without a decimal tail; everything else keeps 4 decimals.
            if (number == static_cast<double>(static_cast<long long>(number)) &&
                number > -1e15 && number < 1e15) {
                return std::to_string(static_cast<long long>(number));
            }
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.4g", number);
            return buffer;
        }
        case Type::String:
            return string;
        case Type::Array: {
            std::string out = "[";
            if (array) {
                for (std::size_t i = 0; i < array->items.size(); ++i) {
                    if (i > 0) out += ", ";
                    out += array->items[i].ToString();
                }
            }
            out += "]";
            return out;
        }
        case Type::Function:
            return function ? "<fn " + function->name + ">" : "<fn>";
    }
    return "nil";
}

std::string Value::TypeName() const {
    switch (type) {
        case Type::Nil: return "nil";
        case Type::Bool: return "bool";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Array: return "array";
        case Type::Function: return "function";
    }
    return "nil";
}

namespace {

struct Keyword {
    const char* word;
    TokenType type;
};

const Keyword kKeywords[] = {
    {"let", TokenType::Let},       {"var", TokenType::Var},         {"fn", TokenType::Fn},
    {"return", TokenType::Return}, {"if", TokenType::If},           {"else", TokenType::Else},
    {"while", TokenType::While},   {"for", TokenType::For},         {"in", TokenType::In},
    {"break", TokenType::Break},   {"continue", TokenType::Continue}, {"true", TokenType::True},
    {"false", TokenType::False},   {"nil", TokenType::Nil},
};

bool IsIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool IsIdentifierPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

class Lexer {
public:
    Lexer(std::string_view source, Diagnostic* diagnostic) : source_(source), diagnostic_(diagnostic) {}

    std::vector<Token> Run() {
        std::vector<Token> tokens;
        while (!AtEnd()) {
            StartToken();
            const char c = Advance();
            switch (c) {
                case ' ':
                case '\t':
                case '\r':
                    break;
                case '\n':
                    ++line_;
                    lineStart_ = position_;
                    break;
                case '(': Emit(tokens, TokenType::LParen); break;
                case ')': Emit(tokens, TokenType::RParen); break;
                case '{': Emit(tokens, TokenType::LBrace); break;
                case '}': Emit(tokens, TokenType::RBrace); break;
                case '[': Emit(tokens, TokenType::LBracket); break;
                case ']': Emit(tokens, TokenType::RBracket); break;
                case ',': Emit(tokens, TokenType::Comma); break;
                case '.': Emit(tokens, TokenType::Dot); break;
                case ';': Emit(tokens, TokenType::Semicolon); break;
                case ':': Emit(tokens, TokenType::Colon); break;
                case '+': Emit(tokens, TokenType::Plus); break;
                case '-': Emit(tokens, TokenType::Minus); break;
                case '*': Emit(tokens, TokenType::Star); break;
                case '%': Emit(tokens, TokenType::Percent); break;
                case '/':
                    if (Match('/')) {
                        while (!AtEnd() && Peek() != '\n') Advance();
                    } else if (Match('*')) {
                        while (!AtEnd() && !(Peek() == '*' && PeekNext() == '/')) {
                            if (Peek() == '\n') {
                                ++line_;
                                lineStart_ = position_ + 1;
                            }
                            Advance();
                        }
                        if (!AtEnd()) {
                            Advance();
                            Advance();
                        }
                    } else {
                        Emit(tokens, TokenType::Slash);
                    }
                    break;
                case '!':
                    Emit(tokens, Match('=') ? TokenType::NotEqual : TokenType::Bang);
                    break;
                case '=':
                    Emit(tokens, Match('=') ? TokenType::EqualEqual : TokenType::Assign);
                    break;
                case '<':
                    Emit(tokens, Match('=') ? TokenType::LessEqual : TokenType::Less);
                    break;
                case '>':
                    Emit(tokens, Match('=') ? TokenType::GreaterEqual : TokenType::Greater);
                    break;
                case '&':
                    if (Match('&')) Emit(tokens, TokenType::AndAnd);
                    else Fail("unexpected '&' (use '&&')");
                    break;
                case '|':
                    if (Match('|')) Emit(tokens, TokenType::OrOr);
                    else Fail("unexpected '|' (use '||')");
                    break;
                case '"':
                case '\'':
                    String(tokens, c);
                    break;
                default:
                    if (std::isdigit(static_cast<unsigned char>(c))) {
                        Number(tokens);
                    } else if (IsIdentifierStart(c)) {
                        Identifier(tokens);
                    } else {
                        Fail(std::string("unexpected character '") + c + "'");
                    }
                    break;
            }
            if (failed_) return tokens;
        }
        Token end;
        end.type = TokenType::End;
        end.line = line_;
        end.column = position_ - lineStart_ + 1;
        tokens.push_back(end);
        return tokens;
    }

    const std::string& error() const { return error_; }

private:
    bool AtEnd() const { return position_ >= source_.size(); }
    char Peek() const { return position_ < source_.size() ? source_[position_] : '\0'; }
    char PeekNext() const {
        return position_ + 1 < source_.size() ? source_[position_ + 1] : '\0';
    }
    char Advance() { return source_[position_++]; }
    bool Match(char expected) {
        if (AtEnd() || source_[position_] != expected) return false;
        ++position_;
        return true;
    }
    void StartToken() {
        tokenStart_ = position_;
        tokenLine_ = line_;
        tokenColumn_ = position_ - lineStart_ + 1;
    }
    void Emit(std::vector<Token>& tokens, TokenType type) {
        Token token;
        token.type = type;
        token.text = std::string(source_.substr(tokenStart_, position_ - tokenStart_));
        token.line = tokenLine_;
        token.column = tokenColumn_;
        tokens.push_back(std::move(token));
    }
    void Fail(const std::string& message) {
        failed_ = true;
        error_ = message;
        if (diagnostic_) {
            diagnostic_->message = message;
            diagnostic_->line = tokenLine_;
            diagnostic_->column = tokenColumn_;
        }
    }
    void String(std::vector<Token>& tokens, char quote) {
        std::string text;
        while (!AtEnd() && Peek() != quote) {
            char c = Advance();
            if (c == '\\' && !AtEnd()) {
                const char escape = Advance();
                switch (escape) {
                    case 'n': text.push_back('\n'); break;
                    case 't': text.push_back('\t'); break;
                    case '\\': text.push_back('\\'); break;
                    case '"': text.push_back('"'); break;
                    case '\'': text.push_back('\''); break;
                    default: text.push_back(escape); break;
                }
                continue;
            }
            if (c == '\n') ++line_;
            text.push_back(c);
        }
        if (AtEnd()) {
            Fail("unterminated string");
            return;
        }
        Advance();  // closing quote
        Token token;
        token.type = TokenType::String;
        token.text = text;
        token.line = tokenLine_;
        token.column = tokenColumn_;
        tokens.push_back(std::move(token));
    }
    void Number(std::vector<Token>& tokens) {
        while (!AtEnd() && (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '_')) Advance();
        if (!AtEnd() && Peek() == '.' && std::isdigit(static_cast<unsigned char>(PeekNext()))) {
            Advance();
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) Advance();
        }
        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
            Advance();
            if (!AtEnd() && (Peek() == '+' || Peek() == '-')) Advance();
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) Advance();
        }
        Token token;
        token.type = TokenType::Number;
        token.text = std::string(source_.substr(tokenStart_, position_ - tokenStart_));
        std::string cleaned;
        for (char c : token.text) {
            if (c != '_') cleaned.push_back(c);
        }
        token.number = std::strtod(cleaned.c_str(), nullptr);
        token.line = tokenLine_;
        token.column = tokenColumn_;
        tokens.push_back(std::move(token));
    }
    void Identifier(std::vector<Token>& tokens) {
        while (!AtEnd() && IsIdentifierPart(Peek())) Advance();
        const std::string text(source_.substr(tokenStart_, position_ - tokenStart_));
        Token token;
        token.type = TokenType::Identifier;
        for (const auto& keyword : kKeywords) {
            if (text == keyword.word) {
                token.type = keyword.type;
                break;
            }
        }
        token.text = text;
        token.line = tokenLine_;
        token.column = tokenColumn_;
        tokens.push_back(std::move(token));
    }

    std::string_view source_;
    Diagnostic* diagnostic_ = nullptr;
    std::size_t position_ = 0;
    std::size_t lineStart_ = 0;
    std::size_t tokenStart_ = 0;
    int line_ = 1;
    int tokenLine_ = 1;
    int tokenColumn_ = 1;
    bool failed_ = false;
    std::string error_;
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------
class Parser {
public:
    Parser(std::vector<Token> tokens, Diagnostic* diagnostic)
        : tokens_(std::move(tokens)), diagnostic_(diagnostic) {}

    std::vector<StmtPtr> ParseProgram() {
        std::vector<StmtPtr> statements;
        while (!AtEnd() && !failed_) {
            StmtPtr statement = Declaration();
            if (statement) statements.push_back(std::move(statement));
        }
        return statements;
    }
    bool failed() const { return failed_; }

private:
    const Token& Current() const { return tokens_[current_]; }
    const Token& Previous() const { return tokens_[current_ - 1]; }
    bool AtEnd() const { return Current().type == TokenType::End; }
    bool Check(TokenType type) const { return Current().type == type; }
    bool Match(TokenType type) {
        if (!Check(type)) return false;
        ++current_;
        return true;
    }
    const Token& Advance() {
        if (!AtEnd()) ++current_;
        return Previous();
    }
    void Fail(const std::string& message) {
        if (failed_) return;
        failed_ = true;
        if (diagnostic_) {
            diagnostic_->message = message;
            diagnostic_->line = Current().line;
            diagnostic_->column = Current().column;
        }
    }
    const Token& Expect(TokenType type, const char* what) {
        if (Check(type)) return Advance();
        Fail(std::string("expected ") + what);
        return Current();
    }
    void ConsumeOptionalSemicolon() { Match(TokenType::Semicolon); }

    StmtPtr Declaration() {
        if (Match(TokenType::Fn)) return FunctionDeclaration();
        return Statement();
    }

    StmtPtr FunctionDeclaration() {
        auto statement = std::make_unique<Stmt>();
        statement->kind = StmtKind::Function;
        statement->line = Previous().line;
        const Token& name = Expect(TokenType::Identifier, "a function name");
        statement->name = name.text;
        Expect(TokenType::LParen, "'('");
        if (!Check(TokenType::RParen)) {
            do {
                const Token& parameter = Expect(TokenType::Identifier, "a parameter name");
                statement->params.push_back(parameter.text);
            } while (Match(TokenType::Comma));
        }
        Expect(TokenType::RParen, "')'");
        statement->body = Block();
        return statement;
    }

    StmtPtr Statement() {
        if (Match(TokenType::Let)) return LetDeclaration(false);
        if (Match(TokenType::Var)) return LetDeclaration(true);
        if (Match(TokenType::If)) return IfStatement();
        if (Match(TokenType::While)) return WhileStatement();
        if (Match(TokenType::For)) return ForStatement();
        if (Match(TokenType::Return)) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = StmtKind::Return;
            statement->line = Previous().line;
            if (!Check(TokenType::RBrace) && !Check(TokenType::Semicolon) && !AtEnd()) {
                statement->expression = Expression();
            }
            ConsumeOptionalSemicolon();
            return statement;
        }
        if (Match(TokenType::Break)) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = StmtKind::Break;
            statement->line = Previous().line;
            ConsumeOptionalSemicolon();
            return statement;
        }
        if (Match(TokenType::Continue)) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = StmtKind::Continue;
            statement->line = Previous().line;
            ConsumeOptionalSemicolon();
            return statement;
        }
        if (Match(TokenType::LBrace)) {
            auto statement = std::make_unique<Stmt>();
            statement->kind = StmtKind::Block;
            statement->line = Previous().line;
            statement->body = BlockContents();
            return statement;
        }
        return ExpressionStatement();
    }

    std::vector<StmtPtr> Block() {
        Expect(TokenType::LBrace, "'{'");
        return BlockContents();
    }

    std::vector<StmtPtr> BlockContents() {
        std::vector<StmtPtr> statements;
        while (!Check(TokenType::RBrace) && !AtEnd() && !failed_) {
            StmtPtr statement = Declaration();
            if (statement) statements.push_back(std::move(statement));
        }
        Expect(TokenType::RBrace, "'}'");
        return statements;
    }

    StmtPtr LetDeclaration(bool mutableBinding) {
        auto statement = std::make_unique<Stmt>();
        statement->kind = StmtKind::Let;
        statement->line = Previous().line;
        statement->mutableBinding = mutableBinding;
        const Token& name = Expect(TokenType::Identifier, "a variable name");
        statement->name = name.text;
        if (Match(TokenType::Assign)) statement->expression = Expression();
        ConsumeOptionalSemicolon();
        return statement;
    }

    StmtPtr IfStatement() {
        auto statement = std::make_unique<Stmt>();
        statement->kind = StmtKind::If;
        statement->line = Previous().line;
        statement->expression = Expression();
        statement->body = Block();
        if (Match(TokenType::Else)) {
            if (Check(TokenType::If)) {
                Advance();
                statement->elseBody.push_back(IfStatement());
            } else {
                statement->elseBody = Block();
            }
        }
        return statement;
    }

    StmtPtr WhileStatement() {
        auto statement = std::make_unique<Stmt>();
        statement->kind = StmtKind::While;
        statement->line = Previous().line;
        statement->expression = Expression();
        statement->body = Block();
        return statement;
    }

    StmtPtr ForStatement() {
        auto statement = std::make_unique<Stmt>();
        statement->kind = StmtKind::ForIn;
        statement->line = Previous().line;
        // `for (i in xs)` and `for i in xs` both work; the binding is scoped to the body.
        const bool parenthesised = Match(TokenType::LParen);
        Match(TokenType::Let) || Match(TokenType::Var);
        const Token& name = Expect(TokenType::Identifier, "a loop variable");
        statement->name = name.text;
        statement->hasForVariable = true;
        Expect(TokenType::In, "'in'");
        statement->expression = Expression();
        if (parenthesised) Expect(TokenType::RParen, "')'");
        statement->body = Block();
        return statement;
    }

    StmtPtr ExpressionStatement() {
        ExprPtr expression = Expression();
        if (failed_) return nullptr;
        auto statement = std::make_unique<Stmt>();
        statement->line = expression->line;
        if (expression->kind == ExprKind::Binary && expression->op == TokenType::Assign) {
            statement->kind = StmtKind::Assign;
            statement->assignTarget = std::move(expression->left);
            statement->expression = std::move(expression->right);
        } else {
            statement->kind = StmtKind::Expression;
            statement->expression = std::move(expression);
        }
        ConsumeOptionalSemicolon();
        return statement;
    }

    ExprPtr Expression() { return Assignment(); }

    ExprPtr Assignment() {
        ExprPtr left = Or();
        if (failed_) return nullptr;
        if (Match(TokenType::Assign)) {
            ExprPtr right = Assignment();
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->op = TokenType::Assign;
            binary->line = left->line;
            binary->left = std::move(left);
            binary->right = std::move(right);
            return binary;
        }
        return left;
    }

    ExprPtr Or() {
        ExprPtr left = And();
        while (Match(TokenType::OrOr)) {
            left = MakeLogical(TokenType::OrOr, std::move(left), And());
        }
        return left;
    }
    ExprPtr And() {
        ExprPtr left = Equality();
        while (Match(TokenType::AndAnd)) {
            left = MakeLogical(TokenType::AndAnd, std::move(left), Equality());
        }
        return left;
    }
    ExprPtr Equality() {
        ExprPtr left = Comparison();
        while (Check(TokenType::EqualEqual) || Check(TokenType::NotEqual)) {
            const TokenType op = Advance().type;
            left = MakeBinary(op, std::move(left), Comparison());
        }
        return left;
    }
    ExprPtr Comparison() {
        ExprPtr left = Term();
        while (Check(TokenType::Less) || Check(TokenType::LessEqual) ||
               Check(TokenType::Greater) || Check(TokenType::GreaterEqual)) {
            const TokenType op = Advance().type;
            left = MakeBinary(op, std::move(left), Term());
        }
        return left;
    }
    ExprPtr Term() {
        ExprPtr left = Factor();
        while (Check(TokenType::Plus) || Check(TokenType::Minus)) {
            const TokenType op = Advance().type;
            left = MakeBinary(op, std::move(left), Factor());
        }
        return left;
    }
    ExprPtr Factor() {
        ExprPtr left = Unary();
        while (Check(TokenType::Star) || Check(TokenType::Slash) || Check(TokenType::Percent)) {
            const TokenType op = Advance().type;
            left = MakeBinary(op, std::move(left), Unary());
        }
        return left;
    }
    ExprPtr Unary() {
        if (Check(TokenType::Bang) || Check(TokenType::Minus)) {
            const TokenType op = Advance().type;
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::Unary;
            expression->op = op;
            expression->line = Previous().line;
            expression->right = Unary();
            return expression;
        }
        return Call();
    }
    ExprPtr Call() {
        ExprPtr expression = Primary();
        while (!failed_) {
            if (Match(TokenType::LParen)) {
                auto call = std::make_unique<Expr>();
                call->kind = ExprKind::Call;
                call->line = expression->line;
                call->target = std::move(expression);
                if (!Check(TokenType::RParen)) {
                    do {
                        call->arguments.push_back(Expression());
                    } while (Match(TokenType::Comma));
                }
                Expect(TokenType::RParen, "')'");
                expression = std::move(call);
            } else if (Match(TokenType::LBracket)) {
                auto index = std::make_unique<Expr>();
                index->kind = ExprKind::Index;
                index->line = expression->line;
                index->target = std::move(expression);
                index->right = Expression();
                Expect(TokenType::RBracket, "']'");
                expression = std::move(index);
            } else if (Match(TokenType::Dot)) {
                const Token& property = Expect(TokenType::Identifier, "a property name");
                auto member = std::make_unique<Expr>();
                member->kind = ExprKind::Member;
                member->line = expression->line;
                member->name = property.text;
                member->target = std::move(expression);
                expression = std::move(member);
            } else {
                break;
            }
        }
        return expression;
    }
    ExprPtr Primary() {
        if (Match(TokenType::Number)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::Literal;
            expression->line = Previous().line;
            expression->literal = Value(Previous().number);
            return expression;
        }
        if (Match(TokenType::String)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::Literal;
            expression->line = Previous().line;
            expression->literal = Value(Previous().text);
            return expression;
        }
        if (Match(TokenType::True) || Match(TokenType::False)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::Literal;
            expression->line = Previous().line;
            expression->literal = Value(Previous().type == TokenType::True);
            return expression;
        }
        if (Match(TokenType::Nil)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::Literal;
            expression->line = Previous().line;
            expression->literal = Value(nullptr);
            return expression;
        }
        if (Match(TokenType::Identifier)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::Variable;
            expression->line = Previous().line;
            expression->name = Previous().text;
            return expression;
        }
        if (Match(TokenType::LParen)) {
            ExprPtr expression = Expression();
            Expect(TokenType::RParen, "')'");
            return expression;
        }
        if (Match(TokenType::LBracket)) {
            auto expression = std::make_unique<Expr>();
            expression->kind = ExprKind::ArrayLiteral;
            expression->line = Previous().line;
            if (!Check(TokenType::RBracket)) {
                do {
                    expression->elements.push_back(Expression());
                } while (Match(TokenType::Comma));
            }
            Expect(TokenType::RBracket, "']'");
            return expression;
        }
        Fail("expected an expression");
        auto expression = std::make_unique<Expr>();
        expression->kind = ExprKind::Literal;
        expression->line = Current().line;
        expression->literal = Value(nullptr);
        return expression;
    }

    static ExprPtr MakeBinary(TokenType op, ExprPtr left, ExprPtr right) {
        auto expression = std::make_unique<Expr>();
        expression->kind = ExprKind::Binary;
        expression->op = op;
        expression->line = left->line;
        expression->left = std::move(left);
        expression->right = std::move(right);
        return expression;
    }
    static ExprPtr MakeLogical(TokenType op, ExprPtr left, ExprPtr right) {
        auto expression = std::make_unique<Expr>();
        expression->kind = ExprKind::Logical;
        expression->op = op;
        expression->line = left->line;
        expression->left = std::move(left);
        expression->right = std::move(right);
        return expression;
    }

    std::vector<Token> tokens_;
    Diagnostic* diagnostic_ = nullptr;
    std::size_t current_ = 0;
    bool failed_ = false;
};

}  // namespace

std::vector<Token> Tokenize(std::string_view source, Diagnostic* diagnostic) {
    Lexer lexer(source, diagnostic);
    std::vector<Token> tokens = lexer.Run();
    if (diagnostic && diagnostic->message.empty() && !lexer.error().empty()) {
        diagnostic->message = lexer.error();
        diagnostic->line = 1;
        diagnostic->column = 1;
    }
    return tokens;
}

Program Program::Parse(std::string_view source, Diagnostic* diagnostic) {
    Program program;
    Diagnostic lexerDiagnostic;
    std::vector<Token> tokens = Tokenize(source, &lexerDiagnostic);
    if (!lexerDiagnostic.message.empty()) {
        if (diagnostic) *diagnostic = lexerDiagnostic;
        return program;
    }
    Parser parser(std::move(tokens), diagnostic);
    program.statements = parser.ParseProgram();
    if (parser.failed() && diagnostic && diagnostic->message.empty()) {
        diagnostic->message = "parse error";
    }
    return program;
}

}  // namespace astra::script
