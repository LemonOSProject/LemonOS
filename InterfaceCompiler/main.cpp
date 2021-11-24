#include <cstdio>
#include <cassert> 
#include <stack>
#include <iostream>
#include <fstream>
#include <sstream>

#include "interfacec.h"

std::vector<Code> Codes;
std::vector<std::shared_ptr<Statement>> statements;

void BuildCodes(std::string& input){
    int lineNum = 1;
    int linePos = 0;

    std::string buf;
    for(auto it = input.begin(); it != input.end(); it++){
        char c = *it;
        linePos++;

        auto appendIdentifier = [lineNum, linePos](std::string& buf) { 
            if(buf.length()){
                Codes.push_back(Code(lineNum, linePos, CodeIdentifier, buf));
                buf.clear();
            }
        };

        switch (c)
        {   
        case ',':
            appendIdentifier(buf);
            Codes.push_back(Code(lineNum, linePos, CodeComma));
            break;
        case ')':
            appendIdentifier(buf);
            Codes.push_back(Code(lineNum, linePos, CodeRightParens));
            break;
        case '(':
            appendIdentifier(buf);
            Codes.push_back(Code(lineNum, linePos, CodeLeftParens));
            break;
        case '}':
            appendIdentifier(buf);
            Codes.push_back(Code(lineNum, linePos, CodeRightBrace));
            break;
        case '{':
            appendIdentifier(buf);
            Codes.push_back(Code(lineNum, linePos, CodeLeftBrace));
            break;
        case '\n':
            appendIdentifier(buf);
            Codes.push_back(Code(lineNum, linePos, CodeEndline));
            lineNum++;
            linePos = 0;
            break;
        case '-':
            appendIdentifier(buf);

            if(*(it + 1) == '>'){ // Response Code is '->' so look ahead by one character
                Codes.push_back(Code(lineNum, linePos, CodeResponse));
                it++;
            } else {
                printf("error: [line %d:%d] Unsupported  Unicode character '%c'\n", lineNum, linePos, c);
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
                printf("error: [line %d:%d] Unsupported character! '%c'\n", lineNum, linePos, c);
                exit(2);
            }
            break;
        }
    }

    for(Code& tok : Codes){
        if(tok.type == CodeIdentifier){
            for(auto keyword : keywordCodes){
                if(!tok.value.compare(keyword)){
                    tok.type = CodeKeyword;
                    break;
                }
            }
        }
    }
}

// interface IDENTIFIER {
std::shared_ptr<InterfaceDeclarationStatement> ParseInterfaceDeclarationStatement(std::vector<Code>::iterator& it){
    std::string ifName = "";

    Code& nameTok = *it;

    if(nameTok.type != CodeIdentifier){
        printf("error: [line %d:%d] Invalid Code '%s'\n", nameTok.lineNum, nameTok.linePos, CodeNames[nameTok.type].c_str());
        exit(4);
    }

    ifName = nameTok.value;

    Code& enterTok = *(++it);

    if(enterTok.type != CodeLeftBrace){
        printf("error: [line %d:%d] Invalid Code '%s'\n", nameTok.lineNum, nameTok.linePos, CodeNames[nameTok.type].c_str());
        exit(4);
    }

    return std::shared_ptr<InterfaceDeclarationStatement>(new InterfaceDeclarationStatement(ifName));
}

// (TYPE IDENTIFIER, ...)
ParameterList ParseParameterListStatement(const std::vector<Code>::iterator& end, std::vector<Code>::iterator& it){
    std::vector<std::pair<Type, std::string>> parameters;

    while(it != end){
        Code tok = *(it++);
        Type type;

        if(tok.type == CodeIdentifier && IsType(tok.value)){ // Check if the Code is a typename
            type = types.at(tok.value);
        } else if(tok.type == CodeRightParens) { // Check if list is beign terminated
            return ParameterList(parameters);
        } else if(tok.type == CodeEndline) {
            continue; // Eat line ending
        } else {
            printf("error: [line %d:%d] Invalid type '%s'!", tok.lineNum, tok.linePos, tok.value.c_str());
            exit(4);
        }

        Code idTok = *(it++);
        while(it != end && idTok.type == CodeEndline) idTok = *(it++); // Eat line endings

        if(idTok.type != CodeIdentifier){
            printf("error: [line %d:%d] Unexpected code '%s', expected identifier!", idTok.lineNum, idTok.linePos, CodeNames[idTok.type].c_str());
            exit(4);
        }

        Code nextTok = *(it++);
        while(it != end && nextTok.type == CodeEndline) nextTok = *(it++); // Eat line endings

        if(nextTok.type == CodeComma){
            parameters.push_back({type, idTok.value});
            continue;
        } else if(nextTok.type == CodeRightParens) { // Check if list is beign terminated
            parameters.push_back({type, idTok.value});
            return ParameterList(parameters);
        } else {
            printf("error: [line %d:%d] Unexpected code '%s'!", nextTok.lineNum, nextTok.linePos, CodeNames[nextTok.type].c_str());
            exit(4);
        }
    }
    
    printf("error: Unterminated parameter list! (if you are a end user, wait for a patch)");
    exit(4);
}

void Parse(){
    std::stack<ParserState> states;

    states.push(ParserState(ParserStateRoot));

    auto it = Codes.begin();
    while(it != Codes.end()){
        Code& tok = *it;
        ParserState pState = states.top();

        switch(pState.state){
        case ParserStateRoot:
            switch(tok.type){
            case CodeKeyword: {
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
            } case CodeEndline:
                it++;
                continue;
            default:
                printf("error: [line %d:%d] Unexpected code '%s'!", tok.lineNum, tok.linePos, CodeNames[tok.type].c_str());
                exit(3);
                continue;
            } 
            break;
        case ParserStateInterface:
            switch(tok.type){
            case CodeIdentifier: {
                Code next = *(++it);

                if(next.type == CodeLeftParens){ // Parse parameter list
                    ParameterList pList = ParseParameterListStatement(Codes.end(), ++it);

                    Code response = *(it++);
                    if(response.type == CodeResponse){
                        // Synchronous method
                        if(it->type == CodeLeftParens){ // Check for return parameters
                            it++;

                            ParameterList rPList = ParseParameterListStatement(Codes.end(), it);

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
                    } else if(response.type == CodeEndline){ // New method on a new line
                        // Asynchronous method
                        std::shared_ptr method = std::shared_ptr<ASynchronousMethod>(new ASynchronousMethod(tok.value));
                        method->parameters = pList;

                        auto ifStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(states.top().statement);
                        ifStatement->children.push_back(std::static_pointer_cast<Statement, ASynchronousMethod>(method));
                    } else {
                        printf("error: [line %d] Unexpected Code '%s'!", tok.lineNum, CodeNames[tok.type].c_str());
                        exit(3);
                    }
                } else {
                    printf("error: [line %d] Unexpected Code '%s'!", tok.lineNum, CodeNames[tok.type].c_str());
                    exit(3);
                }
                break;
            } case CodeRightBrace: {
                auto ifStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(states.top().statement);

                statements.push_back(ifStatement);

                states.pop();

                it++;
                break;
            } case CodeEndline: { // Ignore Line Endings
                it++;
                break;
            } default:
                printf("error: [line %d] Unexpected Code '%s'!", tok.lineNum, CodeNames[tok.type].c_str());
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
    out << "#pragma once\n\n"
        << "#include <Lemon/IPC/Message.h>\n"
        << "#include <Lemon/IPC/Interface.h>\n"
        << "#include <Lemon/IPC/Endpoint.h>\n\n"
        << "#include <Lemon/System/Handle.h>\n\n";

    for(auto& st : statements){
        assert(st.get());
        
        if(st->type == StatementDeclareInterface){
            auto interfaceStatement = std::dynamic_pointer_cast<InterfaceDeclarationStatement, Statement>(st);

            std::stringstream client;

            std::stringstream server;

            client << "class " << interfaceStatement->interfaceName << "Endpoint : public Lemon::Endpoint {\n"
                << "public:\n"
                << "    " << interfaceStatement->interfaceName << "Endpoint(const Lemon::Handle& handle) : Endpoint(std::move(handle)) {}\n"
                << "    " << interfaceStatement->interfaceName << "Endpoint(const std::string& interface) : Endpoint(interface) {}\n\n"
                << "    virtual ~" << interfaceStatement->interfaceName << "Endpoint() = default;\n\n";

			std::stringstream requestIDs; // Request IDs
            std::stringstream responseIDs; // Response IDs

			std::stringstream clientRequests; // Client code for sending requests
            std::stringstream serverRequestHandlers; // Server handlers for requests
            serverRequestHandlers << "    virtual void OnPeerDisconnect(const Lemon::Handle& client) = 0;\n";

            std::stringstream serverRequestCondition; // Swtich statement condition for request
            serverRequestCondition
                << "        case Lemon::MessagePeerDisconnect: {\n"
                << "            OnPeerDisconnect(client);\n"
                << "            return 0;\n"
                << "        } ";

			std::stringstream responses;

			uint64_t nextID = 100;

			requestIDs << "    enum RequestID : uint64_t {\n";
			responseIDs << "    enum ResponseID : uint64_t {\n";

            for(auto& st : interfaceStatement->children){
                switch (st->type)
                {
                case StatementAsynchronousMethod: {
                    auto async = std::dynamic_pointer_cast<ASynchronousMethod, Statement>(st);

                    clientRequests << "    void " << async->methodName << "("; // void NAME (
                    serverRequestHandlers << "    virtual void On" << async->methodName << "(const Lemon::Handle& client";

                    /// For each request, the following code is generated for the server
                    ///
                    /// type1 parameter1
                    /// type... parameter...
                    /// if(m.Decode(parameter1, parameter...)){
                    ///     // Error out   
                    /// }
                    ///
                    /// OnRequest(parameter1, parameter...)

                    serverRequestCondition << "case " << "Request" << async->methodName << ": {\n";

					requestIDs << "        Request" << async->methodName << " = " << nextID++ << ",\n"; // $(interfaceName)$(methodName)% = $(nextID);

                    if(async->parameters.parameters.size()){
                        std::stringstream parameters;
                        std::stringstream parameterDeclarations;
                        auto it = async->parameters.parameters.begin();
                        while(it != async->parameters.parameters.end()){
                            auto& param = *it;

                            try{
                                CppType type = cppTypes[param.first];

                                if(type.constReference){
                                    parameterDeclarations << "const " << type.typeName << "& " << param.second; // TYPE IDENTIFIER
                                } else {
                                    parameterDeclarations << type.typeName << " " << param.second; // TYPE IDENTIFIER
                                }

                                serverRequestCondition << "            " << type.typeName << " " << param.second << ";\n"; // Declare local variables for message decoding

                                parameters << param.second;
                            } catch(const std::out_of_range& e){
                                fprintf(stderr, "No mapping for type %d!\n", param.first);
                                exit(5);
                            }

                            it++;
                            if(it != async->parameters.parameters.end()){
                                parameters << ", "; // Not at the end so add a separator
                                parameterDeclarations << ", "; // Not at the end so add a separator
                            }
                        }
                        
                        std::string parameterDeclarationString = parameterDeclarations.str();
                        std::string parameterString = parameters.str();

                        serverRequestCondition
                            << "            if(m.Decode(" << parameterString << ")) { // Check for error decoding request\n"
                            << "                return 1;\n"
                            << "            }\n\n"
                            << "            On" << async->methodName << "(client, " << parameterString << ");\n"
                            << "            return 0;\n        } ";

                        clientRequests << parameterDeclarationString << ") {\n"
                            << "        uint16_t size = Lemon::Message::GetSize(" << parameterString << ");\n" // Get size of message
                            << "        uint8_t* buffer = new uint8_t[size];\n\n"
                            << "        Lemon::Message m = Lemon::Message(buffer, size, " << interfaceStatement->interfaceName << "::Request" << async->methodName << ", " << parameterString << ");\n" // Create message object using our calculated size and stack buffer
                            << "        Queue(m.id(), m.data(), m.length());\n" // Queue message
                            << "    }\n\n";

                        serverRequestHandlers << ", " << parameterDeclarationString << ") = 0;\n"; // virtual void On$(methodName)(Handle client, $(parameters)) = 0; // Pure virtual function call to the handler
                    } else { // No parameters so avoid all the extra stuff
                        clientRequests << ") {\n"
                            << "        Queue(" << interfaceStatement->interfaceName << "::Request" << async->methodName << ", nullptr, 0);\n"
                            << "    }\n\n";

                        serverRequestHandlers << ") = 0;\n";

                        serverRequestCondition
                            << "            On" << async->methodName << "(client);\n"
                            << "            return 0;\n        } ";
                    }
                    break;
                } case StatementSynchronousMethod: {
                    auto sync = std::dynamic_pointer_cast<SynchronousMethod, Statement>(st);

                    std::stringstream parameters;
                    std::stringstream parameterDeclarations;

                    clientRequests << "    " << interfaceStatement->interfaceName << "::" << sync->methodName << "Response " << sync->methodName << "("; // $(interfaceName)::$(methodName)Response  $(methodName)(
                    serverRequestHandlers << "    virtual void On" << sync->methodName << "(const Lemon::Handle& client"; // $(methodName)Response  $(methodName)(
                    
                    /// For each request, the following code is generated for the server
                    ///
                    /// type1 parameter1
                    /// type... parameter...
                    /// if(m.Decode(parameter1, parameter...)){
                    ///     // Error out   
                    /// }
                    ///
                    /// client.Queue(Lemon::Message(responseID, OnRequest(parameter1, parameter...));

                    serverRequestCondition << "case " << "Request" << sync->methodName << ": {\n";

					requestIDs << "        Request" << sync->methodName << " = " << nextID++ << ",\n"; // $(interfaceName)$(methodName)% = $(nextID);
					responseIDs << "        Response" << sync->methodName << " = " << nextID++ << ",\n"; // $(interfaceName)$(methodName)% = $(nextID);
                    
                    if(sync->parameters.parameters.size()){
                        std::stringstream parameters;
                        std::stringstream parameterDeclarations;
                        auto it = sync->parameters.parameters.begin();
                        while(it != sync->parameters.parameters.end()){
                            auto& param = *it;

                            try{
                                CppType type = cppTypes[param.first];

                                if(type.constReference){
                                    parameterDeclarations << "const " << type.typeName << "& " << param.second; // TYPE IDENTIFIER
                                } else {
                                    parameterDeclarations << type.typeName << " " << param.second; // TYPE IDENTIFIER
                                }

                                serverRequestCondition << "            " << type.typeName << " " << param.second << ";\n"; // Declare local variables for message decoding

                                parameters << param.second;
                            } catch(const std::out_of_range& e){
                                fprintf(stderr, "No mapping for type %d!\n", param.first);
                                exit(5);
                            }

                            it++;
                            if(it != sync->parameters.parameters.end()){
                                parameters << ", "; // Not at the end so add a separator
                                parameterDeclarations << ", "; // Not at the end so add a separator
                            }
                        }
                        
                        std::string parameterDeclarationString = parameterDeclarations.str();
                        std::string parameterString = parameters.str();

                        serverRequestCondition
                            << "            if(m.Decode(" << parameterString << ")) { // Check for error decoding request\n"
                            << "                return 1;\n"
                            << "            }\n\n"
                            << "            On" << sync->methodName << "(client, " << parameterString << "); // It is expected that the handler sends the response\n"
                            << "            return 0;\n        } ";

                        clientRequests << parameterDeclarationString << ") {\n"
                            << "        uint16_t size = Lemon::Message::GetSize(" << parameterString << ");\n"
                            << "        uint8_t* buffer = new uint8_t[m_msgSize];\n\n"
                            << "        Lemon::Message m = Lemon::Message(buffer, size, " << interfaceStatement->interfaceName << "::Request" << sync->methodName << ", " << parameterString << ");\n"
                            << "        if(Call(m, " << interfaceStatement->interfaceName << "::Response" << sync->methodName << ")) throw std::runtime_error(\"Failed calling " << sync->methodName << "\");\n\n"
                            << "        " << interfaceStatement->interfaceName << "::" << sync->methodName << "Response response;\n" // $(methodName)Response response;
                            << "        if(m.Decode(";
                            
                        // The struct may not be trivally copyable,
                        // We pass each member individually
                        for(auto param = sync->returnParameters.parameters.begin(); param != sync->returnParameters.parameters.end();){
                            clientRequests << "response." << param->second;

                            param++;
                            // Make sure we dont insert a comma at the end
                            if(param != sync->returnParameters.parameters.end()){
                                clientRequests << ", ";
                            }
                        }

                        clientRequests << ")){\n"
                            << "            throw std::runtime_error(\"Invalid response to request " << sync->methodName << "!\");\n"
                            << "            return response; // Error decoding response\n"
                            << "        }\n\n"
                            << "        return response;\n"
                            << "    }\n\n";

                        serverRequestHandlers << ", " << parameterDeclarationString << ") = 0;\n";
                    } else {
                        clientRequests << ") {\n"
                            << "        uint8_t* buffer = new uint8_t[m_msgSize];\n\n"
                            << "        Lemon::Message m = Lemon::Message(buffer, 0, " << interfaceStatement->interfaceName << "::Request" << sync->methodName << ");\n"
                            << "        if(Call(m, " << interfaceStatement->interfaceName << "::Response" << sync->methodName << ")) throw std::runtime_error(\"Failed calling " << sync->methodName << "\");\n\n"
                            << "        " << interfaceStatement->interfaceName << "::" << sync->methodName << "Response response;\n" // $(methodName)Response response;
                            << "        if(m.Decode(";
                            
                        // The struct may not be trivally copyable,
                        // We pass each member individually
                        for(auto param = sync->returnParameters.parameters.begin(); param != sync->returnParameters.parameters.end();){
                            clientRequests << "response." << param->second;

                            param++;
                            // Make sure we dont insert a comma at the end
                            if(param != sync->returnParameters.parameters.end()){
                                clientRequests << ", ";
                            }
                        }

                        clientRequests << ")){\n"
                            << "            throw std::runtime_error(\"Invalid response to request " << sync->methodName << "!\");\n"
                            << "            return response; // Error decoding response\n"
                            << "        }\n\n"
                            << "        return response;\n"
                            << "    }\n\n";
                        
                        serverRequestHandlers << ") = 0;\n";

                        serverRequestCondition
                            << "            On" << sync->methodName << "(client);\n"
                            << "            return 0;\n        } ";
                    }

					responses << "    struct " << sync->methodName << "Response {\n    "; // struct $(methodName)Response { $(parameterType) $(parameterName) ... };
					for(auto& param : sync->returnParameters.parameters){
                        try{
                            CppType type = cppTypes[param.first];

                            responses << "    " << type.typeName << " " << param.second << ";\n    ";
                        } catch(const std::out_of_range& e){
                            fprintf(stderr, "No mapping for type %d!\n", param.first);
                            exit(5);
                        }
					}
					responses << "};\n\n";
                    break;
                } default:
                    break;
                }
            }

			requestIDs << "    };\n\n";
			responseIDs << "    };\n\n";

			client << clientRequests.rdbuf();
            client << "};\n";


            server << "class " << interfaceStatement->interfaceName << " {\n"
                << "public:\n"
                << "    virtual ~" << interfaceStatement->interfaceName << "() = default;\n\n"
			    << requestIDs.rdbuf();

            if(responses.tellp() > 0){ // Check that there are actually responses
                server
                    << responseIDs.rdbuf()
                    << responses.rdbuf();
            }

            server
                << "protected:\n"
                << serverRequestHandlers.rdbuf()
                << "\n";

            server
                << "    int HandleMessage(const Lemon::Handle& client, const Lemon::Message& m){\n"
                << "        switch(m.id()) {\n"
                << serverRequestCondition.rdbuf()
                << "\n        }\n"
                << "    }\n"
                << "};\n";

            out << server.rdbuf() << std::endl;
            out << client.rdbuf() << std::endl;
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        printf("Usage: %s <file> [outFile]\n", argv[0]);
        exit(1);
    }

    FILE* inputFile;
    if(!(inputFile = fopen(argv[1], "r"))){
        perror("Error opening file for reading: ");
        exit(1);
    }

    std::ofstream outFile;
    if(argc >= 3){
        outFile.open(argv[2]);
        
        if(!outFile.is_open()){
            perror("Error opening file for writing!");
            exit(1);
        }
    }

    std::string input;

    fseek(inputFile, 0, SEEK_END);
    size_t inputSz = ftell(inputFile);

    input.resize(inputSz);
    fseek(inputFile, 0, SEEK_SET);

    fread(&input.front(), 1, inputSz, inputFile);

    BuildCodes(input);

    Parse();

    if(outFile.is_open()){
        Generate(outFile);
    } else {
        Generate(std::cout);
    }

    return 0;
}