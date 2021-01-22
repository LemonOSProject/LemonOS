#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <utility>

// Asynchronous remote procedure: IDENTIFIER PARAMETERS

// Synchronous remote procedure: IDENTIFIER PARAMETERS -> PARAMETERS
// Protocol: protocol IDENTIFIER { }
// Parameters: (IDENTIFIER TYPE, ...)

enum TokenType{
    TokenEndline,
    TokenIdentifier,
    TokenKeyword,
    TokenLeftBrace,
    TokenRightBrace,
    TokenLeftParens,
    TokenRightParens,
    TokenComma,
    TokenResponse,
};

enum Type{
    TypeString,
    TypeBool,
    TypeU8,
    TypeU16,
    TypeU32,
    TypeU64,
    TypeS8,
    TypeS16,
    TypeS32,
    TypeS64,
};

static std::map<std::string, Type> types = {
    {"string", TypeString},
    {"bool", TypeBool},
    {"u8", TypeU8},
    {"u16", TypeU16},
    {"u32", TypeU32},
    {"u64", TypeU64},
    {"s8", TypeS8},
    {"s16", TypeS16},
    {"s32", TypeS32},
    {"s64", TypeS64},
};

enum KeywordType{
    KeywordInterface,
    KeywordStruct,
    KeywordUnion,
};

static std::vector<std::string> tokenNames = {
    "ENDLINE",
    "IDENTIFIER",
    "KEYWORD",
    "{",
    "}",
    "(",
    ")",
    ",",
    "RESPONSE",
};

enum StatementType {
    StatementInvalid,
    StatementDeclareInterface,
    StatementAsynchronousMethod,
    StatementSynchronousMethod,
    StatementParameterList,
};

#define IsDeclaration(x) ((x == ParserStateDeclarationSync) || (x == ParserStateDeclarationAsync) || (x == ParserStateDeclarationInterface))

class Statement
{
public:
    StatementType type = StatementInvalid;

    Statement() = default;
    Statement(StatementType t) { type = t; }

    virtual ~Statement() = default;
};

class InterfaceDeclarationStatement final : public Statement {
public:
    std::string interfaceName;

    std::vector<std::shared_ptr<Statement>> children;

    InterfaceDeclarationStatement(const std::string& name){
        interfaceName = name;
        type = StatementDeclareInterface;
    }
};

class ParameterList final : public Statement {
public:
    std::vector<std::pair<Type, std::string>> parameters;

    ParameterList(){
        type = StatementParameterList;
    }

    ParameterList(const std::vector<std::pair<Type, std::string>>& params){
        type = StatementParameterList;

        parameters = params;
    }
};

class ASynchronousMethod final : public Statement{
public:
    std::string methodName;

    ParameterList parameters;

    ASynchronousMethod(const std::string& name){
        methodName = name;

        type = StatementAsynchronousMethod;
    }
};

class SynchronousMethod final : public Statement{
public:
    std::string methodName;

    ParameterList parameters;
    ParameterList returnParameters;

    SynchronousMethod(const std::string& name){
        methodName = name;

        type = StatementSynchronousMethod;
    }
};

class Token{
public:
    TokenType type;
    std::string value = "";
    int lineNum;
    int linePos;

    Token(){}

    Token(int ln, int pos, TokenType type){
        lineNum = ln;
        linePos = pos;

        this->type = type;
    }

    Token(int ln, int pos, TokenType type, std::string& text) : value(text) {
        lineNum = ln;
        linePos = pos;

        this->type = type;
    }
};

enum {
    ParserStateInvalid,
    ParserStateRoot,
    ParserStateDeclarationAsync,
    ParserStateDeclarationSync,
    ParserStateInterface,
    ParserStateParameterList,
};

struct ParserState{
    int state = ParserStateInvalid;
    std::shared_ptr<Statement> statement;

    ParserState(int s){
        state = s;
    }
};

static std::vector<std::string> keywordTokens = {
    "interface",
    "struct",
    "union",
};

static std::map<std::string, KeywordType> keywords = {
    {"interface", KeywordInterface},
    {"struct", KeywordStruct},
    {"union", KeywordUnion},
};

bool IsType(const std::string& s){
    return (types.find(s) != types.end());
}