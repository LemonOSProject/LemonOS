#include <cstdio>
#include <cassert> 
#include <stack>
#include <iostream>
#include <sstream>

#include "interfacec.h"

std::vector<Token> tokens;
std::vector<std::shared_ptr<Statement>> statements;

void BuildTokens(std::string& input){
    int lineNum = 1;
    int linePos = 0;

    std::string buf;
    for(auto it = input.begin(); it != input.end(); it++){
        char c = *it;
        linePos++;

        auto appendIdentifier = [lineNum, linePos](std::string& buf) { 
            if(buf.length()){
                tokens.push_back(Token(lineNum, linePos, TokenIdentifier, buf));
                buf.clear();
            }
        };

        switch (c)
        {   
        case ',':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, linePos, TokenComma));
            break;
        case ')':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, linePos, TokenRightParens));
            break;
        case '(':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, linePos, TokenLeftParens));
            break;
        case '}':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, linePos, TokenRightBrace));
            break;
        case '{':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, linePos, TokenLeftBrace));
            break;
        case '\n':
            appendIdentifier(buf);
            tokens.push_back(Token(lineNum, linePos, TokenEndline));
            lineNum++;
            linePos = 0;
            break;
        case '-':
            appendIdentifier(buf);

            if(*(it + 1) == '>'){ // Response token is '->' so look ahead by one character
                tokens.push_back(Token(lineNum, linePos, TokenResponse));
                it++;
            } else {
                printf("error: [line %d:%d] Unsupported character '%c'\n", lineNum, linePos, c);
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
                printf("error: [line %d:%d] Unsupported character '%c'\n", lineNum, linePos, c);
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
        printf("error: [line %d:%d] Invalid token '%s'\n", nameTok.lineNum, nameTok.linePos, tokenNames[nameTok.type]);
        exit(4);
    }

    ifName = nameTok.value;

    Token& enterTok = *(++it);

    if(enterTok.type != TokenLeftBrace){
        printf("error: [line %d:%d] Invalid token '%s'\n", nameTok.lineNum, nameTok.linePos, tokenNames[nameTok.type]);
        exit(4);
    }

    return std::shared_ptr<InterfaceDeclarationStatement>(new InterfaceDeclarationStatement(ifName));
}

// (TYPE IDENTIFIER, ...)
ParameterList ParseParameterListStatement(const std::vector<Token>::iterator& end, std::vector<Token>::iterator& it){
    std::vector<std::pair<Type, std::string>> parameters;

    while(it != end){
        Token tok = *(it++);
        Type type;

        if(tok.type == TokenIdentifier && IsType(tok.value)){ // Check if the token is a typename
            type = types.at(tok.value);
        } else if(tok.type == TokenRightParens) { // Check if list is beign terminated
            return ParameterList(parameters);
        } else if(tok.type == TokenEndline) {
            continue; // Eat line ending
        } else {
            printf("error: [line %d:%d] Invalid type '%s'!", tok.lineNum, tok.linePos, tok.value.c_str());
            exit(4);
        }

        Token idTok = *(it++);
        while(it != end && idTok.type == TokenEndline) idTok = *(it++); // Eat line endings

        if(idTok.type != TokenIdentifier){
            printf("error: [line %d:%d] Unexpected token '%s', expected identifier!", idTok.lineNum, idTok.linePos, tokenNames[idTok.type].c_str());
            exit(4);
        }

        Token nextTok = *(it++);
        while(it != end && nextTok.type == TokenEndline) nextTok = *(it++); // Eat line endings

        if(nextTok.type == TokenComma){
            parameters.push_back({type, idTok.value});
            continue;
        } else if(nextTok.type == TokenRightParens) { // Check if list is beign terminated
            parameters.push_back({type, idTok.value});
            return ParameterList(parameters);
        } else {
            printf("error: [line %d:%d] Unexpected token '%s'!", nextTok.lineNum, nextTok.linePos, tokenNames[nextTok.type].c_str());
            exit(4);
        }
    }
    
    printf("error: Unterminated parameter list!");
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
                    it++;
                    states.push(ParserStateInterface);

                    states.top().statement = ifDecl;
                } else {
                    printf("error: [line %d:%d] Unexpected keyword '%s'!", tok.lineNum, tok.linePos, tok.value.c_str());
                    exit(3);
                }
                break;
            } case TokenEndline:
                it++;
                continue;
            default:
                printf("error: [line %d:%d] Unexpected token '%s'!", tok.lineNum, tok.linePos, tokenNames[tok.type].c_str());
                exit(3);
                continue;
            } 
            break;
        case ParserStateInterface:
            switch(tok.type){
            case TokenIdentifier: {
                Token next = *(++it);

                if(next.type == TokenLeftParens){ // Parse parameter list
                    ParameterList pList = ParseParameterListStatement(tokens.end(), ++it);

                    Token response = *(it++);
                    if(response.type == TokenResponse){
                        // Synchronous method
                        if(it->type == TokenLeftParens){ // Check for return parameters
                            it++;

                            ParameterList rPList = ParseParameterListStatement(tokens.end(), it);

                            std::shared_ptr method = std::shared_ptr<SynchronousMethod>(new SynchronousMethod(tok.value));
                            method->parameters = pList;
                            method->returnParameters = rPList;

                            auto ifStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(states.top().statement);
                            ifStatement->children.push_back(std::static_pointer_cast<Statement, SynchronousMethod>(method));
                            
                        } else { // No return parameters
                            std::shared_ptr method = std::shared_ptr<SynchronousMethod>(new SynchronousMethod(tok.value));
                            method->parameters = pList;

                            auto ifStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(states.top().statement);
                            ifStatement->children.push_back(std::static_pointer_cast<Statement, SynchronousMethod>(method));
                        }
                    } else if(response.type == TokenEndline){ // New method on a new line
                        // Asynchronous method
                        std::shared_ptr method = std::shared_ptr<ASynchronousMethod>(new ASynchronousMethod(tok.value));
                        method->parameters = pList;

                        auto ifStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(states.top().statement);
                        ifStatement->children.push_back(std::static_pointer_cast<Statement, ASynchronousMethod>(method));
                    } else {
                        printf("error: [line %d] Unexpected token '%s'!", tok.lineNum, tokenNames[tok.type].c_str());
                        exit(3);
                    }
                } else {
                    printf("error: [line %d] Unexpected token '%s'!", tok.lineNum, tokenNames[tok.type].c_str());
                    exit(3);
                }
                break;
            } case TokenRightBrace: {
                auto ifStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(states.top().statement);

                statements.push_back(ifStatement);

                states.pop();

                it++;
                break;
            } case TokenEndline: { // Ignore Line Endings
                it++;
                break;
            } default:
                printf("error: [line %d] Unexpected token '%s'!", tok.lineNum, tokenNames[tok.type].c_str());
                exit(3);
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

struct CppType{
    std::string typeName = ""; // e.g. int32_t, std::string
    bool constReference = false; // e.g. pass std::string parameters as std::string&
};

std::map<Type, CppType> cppTypes = {
    {TypeString, {"std::string", true}},
    {TypeBool, {"bool", false}},
    {TypeU8, {"uint8_t", false}},
    {TypeU16, {"uint16_t", false}},
    {TypeU32, {"uint32_t", false}},
    {TypeU64, {"uint64_t", false}},
    {TypeS8, {"int8_t", false}},
    {TypeS16, {"int16_t", false}},
    {TypeS32, {"int32_t", false}},
    {TypeS64, {"int64_t", false}}
};


void Generate(std::ostream& out){
    out << "#include <lemon/ipc/message.h>" << std::endl
        << "#include <lemon/ipc/interface.h>" << std::endl << std::endl;

    for(auto& st : statements){
        assert(st.get());
        
        if(st->type == StatementDeclareInterface){
            auto interfaceStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(st);

            std::stringstream client;
            std::stringstream server;

            client << "class " << interfaceStatement->interfaceName << "Client : public Lemon::Endpoint {" << std::endl;

            client << "public:" << std::endl;
            for(auto& st : interfaceStatement->children){
                switch (st->type)
                {
                case StatementAsynchronousMethod: {
                    auto async = std::dynamic_pointer_cast<ASynchronousMethod, Statement>(st);
                    client << "\tvoid " << async->methodName << "(";

                    auto it = async->parameters.parameters.begin();
                    while(it != async->parameters.parameters.end()){
                        auto& param = *it;

                        try{
                            CppType type = cppTypes[param.first];

                            if(type.constReference){
                                client << "const " << type.typeName << "& " << param.second; // TYPE IDENTIFIER
                            } else {
                                client << type.typeName << " " << param.second; // TYPE IDENTIFIER
                            }
                        } catch(const std::out_of_range& e){
                            fprintf(stderr, "No mapping for type %d!\n", param.first);
                            exit(5);
                        }

                        it++;
                        if(it != async->parameters.parameters.end()){
                            client << ", "; // Not at the end so add a separator
                        }
                    }

                    client << ") const {" << std::endl;

                    client << "\t\tQueue(Message(";

                    it = async->parameters.parameters.begin();
                    while(it != async->parameters.parameters.end()){
                        auto& param = *it;

                        client << param.second;

                        it++;
                        if(it != async->parameters.parameters.end()){
                            client << ", "; // Not at the end so add a separator
                        }
                    }

                    client << "));" << std::endl
                        << "\t}" << std::endl << std::endl;
                    break;
                } case StatementSynchronousMethod: {
                    break;
                } default:
                    break;
                }
            }

            client << "};" << std::endl;

            server << "class " << interfaceStatement->interfaceName << " {}" << std::endl;

            out << server.rdbuf();
            out << client.rdbuf();
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
    
    for(auto& st : statements){
        assert(st.get());
        
        switch(st->type){
            case StatementDeclareInterface:
                for(auto& ifSt : std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(st)->children){
                    assert(ifSt.get());

                    switch(ifSt->type){
                        case StatementSynchronousMethod: {
                            auto method = std::dynamic_pointer_cast<SynchronousMethod, Statement>(ifSt);
                            assert(method.get());

                            printf("Synchronous Method '%s':\n", method->methodName.c_str());
                            for(auto& param : method->parameters.parameters){
                                printf("\t{%d, %s}\n", param.first, param.second.c_str());
                            }
                            break;
                        } case StatementAsynchronousMethod: {
                            auto method = std::dynamic_pointer_cast<ASynchronousMethod, Statement>(ifSt);
                            assert(method.get());

                            printf("Asynchronous Method '%s':\n", method->methodName.c_str());
                            for(auto& param : method->parameters.parameters){
                                printf("\t{%d, %s}\n", param.first, param.second.c_str());
                            }
                            break;
                        }
                    }
                }
                break;
        }
    }

    Generate(std::cout);

    exit(0);
}