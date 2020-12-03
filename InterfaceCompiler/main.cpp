#include <stdio.h> 
#include <string>
#include <vector>
#include <map>
#include <stack>
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
    TypeU8,
    TypeU16,
    TypeU32,
    TypeU64,
    TypeS8,
    TypeS16,
    TypeS32,
    TypeS64,
};

std::map<std::string, Type> types = {
    {"string", TypeString},
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

std::vector<std::string> tokenNames = {
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
    StatementDeclareInterface,
    StatementAsynchronousMethod,
    StatementSynchronousMethod,
    StatementParameterList,
};

#define IsDeclaration(x) ((x == ParserStateDeclarationSync) || (x == ParserStateDeclarationAsync) || (x == ParserStateDeclarationInterface))

struct Statement
{
    StatementType type;

    Statement() = default;
    Statement(StatementType t) { type = t; }
};

struct InterfaceDeclarationStatement final : Statement {
    std::string interfaceName;

    std::vector<Statement> children;

    InterfaceDeclarationStatement(const std::string& name){
        interfaceName = name;
        type = StatementDeclareInterface;
    }
};

struct ParameterList final : Statement {
    std::vector<std::pair<Type, std::string>> parameters;

    ParameterList(const std::vector<std::pair<Type, std::string>>& params){
        type = StatementParameterList;

        parameters = params;
    }
};

struct ASynchronousMethod final : Statement{
    std::string methodName;

    ParameterList parameters;

    ASynchronousMethod(const std::string& name){
        methodName = name;

        type = StatementAsynchronousMethod;
    }
};

struct SynchronousMethod final : Statement{
    std::string methodName;

    ParameterList parameters;
    ParameterList returnParameters;

    SynchronousMethod(const std::string& name){
        methodName = name;

        type = StatementSynchronousMethod;
    }
};

struct Token{
    TokenType type;
    std::string value = "";
    int lineNum;

    Token(){}

    Token(int ln, TokenType type){
        lineNum = ln;
        this->type = type;
    }

    Token(int ln, TokenType type, std::string& text) : value(text) {
        lineNum = ln;
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

std::vector<std::string> keywordTokens = {
    "interface",
    "struct",
    "union",
};

std::map<std::string, KeywordType> keywords = {
    {"interface", KeywordInterface},
    {"struct", KeywordStruct},
    {"union", KeywordUnion},
};

bool IsType(const std::string& s){
    return (types.find(s) != types.end());
}

std::vector<Token> tokens;
std::vector<std::shared_ptr<Statement>> statements;

void BuildTokens(std::string& input){
    int lineNum = 0;

    std::string buf;
    for(auto it = input.begin(); it != input.end(); it++){
        char c = *it;

        auto appendIdentifier = [lineNum](std::string& buf) { 
            if(buf.length()){
                tokens.push_back(Token(lineNum, TokenIdentifier, buf));
                buf.clear();
            }
        };

        switch (c)
        {   
        case ',':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, TokenComma));
            break;
        case ')':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, TokenRightParens));
            break;
        case '(':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, TokenLeftParens));
            break;
        case '}':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, TokenRightBrace));
            break;
        case '{':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, TokenLeftBrace));
            break;
        case '\n':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, TokenEndline));
            lineNum++;
            break;
        case '-':
            appendIdentifier(buf);

            if(*(it + 1) == '>'){ // Response token is '->' so look ahead by one character
                tokens.push_back(Token(lineNum, TokenResponse));
                it++;
            } else {
                printf("error: [line %d] Unsupported character '%c'\n", lineNum, c);
                exit(2);
            }
            break;
        case ' ': // Eat whitespaces
            appendIdentifier(buf);
            break;
        default:
            if(isalnum(c) || c == '_'){ // Must match 0-9A-Za-z_
                buf += c;
            } else {
                printf("error: [line %d] Unsupported character '%c'\n", lineNum, c);
                exit(2);
            }
            break;
        }
    }

    for(Token& tok : tokens){
        if(tok.type == TokenIdentifier){
            for(auto keyword : keywordTokens){
                if(!tok.value.compare(keyword)){
                    tok.type = TokenKeyword;
                    break;
                }
            }
        }
    }
}

// interface IDENTIFIER {
std::shared_ptr<InterfaceDeclarationStatement> ParseInterfaceDeclarationStatement(std::vector<Token>::iterator& it){
    std::string ifName = "";

    Token& nameTok = *it;

    if(nameTok.type != TokenIdentifier){
        printf("error: [line %d] Invalid token '%s'\n", nameTok.lineNum, tokenNames[nameTok.type]);
        exit(4);
    }

    ifName = nameTok.value;

    Token& enterTok = *(++it);

    if(enterTok.type != TokenLeftBrace){
        printf("error: [line %d] Invalid token '%s'\n", nameTok.lineNum, tokenNames[nameTok.type]);
        exit(4);
    }

    return std::shared_ptr<InterfaceDeclarationStatement>(new InterfaceDeclarationStatement(ifName));
}

// (TYPE IDENTIFIER, ...)
ParameterList ParseParameterListStatement(const std::vector<Token>::iterator& end, std::vector<Token>::iterator& it){
    std::vector<std::pair<Type, std::string>> parameters;

    while(it != end){
        Token typeTok = *(it++);
        Type type;

        if(typeTok.type == TokenIdentifier && IsType(typeTok.value)){ // Check if the token is a typename
            type = types.at(typeTok.value);
        } else if(typeTok.type == TokenRightParens) { // Check if list is beign terminated
            return ParameterList(parameters);
        } else {
            printf("error: [line %d] Invalid type '%s'!", typeTok.lineNum, typeTok.value.c_str());
            exit(4);
        }

        Token idTok = *(it++);
        while(it != end && idTok.type == TokenEndline) idTok = *(it++); // Eat line endings

        if(idTok.type != TokenIdentifier){
            printf("error: [line %d] Unexpected token '%s', expected identifier!", typeTok.lineNum, tokenNames[typeTok.type].c_str());
            exit(4);
        }

        parameters.push_back({type, idTok.value});
    }
    
    printf("error: Unterminated parameter list!"));
    exit(4);
}

void Parse(){
    std::stack<ParserState> states;

    states.push(ParserState(ParserStateRoot));

    auto it = tokens.begin();
    while(it != tokens.end()){
        Token& tok = *it;
        ParserState pState = states.top();

        switch(pState.state){
        case ParserStateRoot:
            switch(tok.type){
            case TokenKeyword: {
                KeywordType kw = keywords.at(tok.value); // Get keyword id

                if(kw == KeywordInterface){
                    std::shared_ptr<InterfaceDeclarationStatement> ifDecl = ParseInterfaceDeclarationStatement(++it);

                    printf("Declaring interface: %s\n", ifDecl->interfaceName.c_str());
                    it++;

                    statements.push_back(ifDecl);
                    states.push(ParserStateInterface);

                    states.top().statement = ifDecl;
                } else {
                    printf("error: [line %d] Unexpected keyword '%s'!", tok.lineNum, tok.value.c_str());
                    exit(3);
                }
                break;
            } case TokenEndline:
                it++;
                continue;
            default:
                printf("error: [line %d] Unexpected token '%s'!", tok.lineNum, tokenNames[tok.type].c_str());
                exit(3);
                continue;
            } 
            break;
        case ParserStateInterface:
            switch(tok.type){
            case TokenIdentifier:
                Token next = *(++it);

                if(next.type == TokenLeftParens){
                    ParameterList pList = ParseParameterListStatement(tokens.end(), ++it);

                    Token response = *(it++);
                    if(response.type == TokenResponse){
                        // Synchronous method
                    } else if(response.type == TokenEndline){
                        // Asynchronous method
                        std::shared_ptr method = std::shared_ptr<ASynchronousMethod>(new ASynchronousMethod(tok.value));
                        method->parameters = pList;
                    } else {
                        
                    }
                } else {
                    printf("error: [line %d] Unexpected token '%s'!", tok.lineNum, tokenNames[tok.type].c_str());
                    exit(3);
                }
                break;
            }
            break;
        case ParserStateInvalid:
        default:
            printf("Parser state is invalid!\n");
            exit(99);
            break;
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        printf("Usage: %s <file>\n", argv[0]);
        exit(1);
    }

    FILE* inputFile;
    if(!(inputFile = fopen(argv[1], "r"))){
        perror("Error opening file for reading: ");
        exit(1);
    }

    std::string input;

    fseek(inputFile, 0, SEEK_END);
    size_t inputSz = ftell(inputFile);

    input.resize(inputSz);
    fseek(inputFile, 0, SEEK_SET);

    fread(&input.front(), 1, inputSz, inputFile);

    BuildTokens(input);

    Parse();

    exit(0);
}