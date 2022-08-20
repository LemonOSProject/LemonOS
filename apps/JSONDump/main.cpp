#include <Lemon/Core/JSON.h>

#include <iostream>
#include <fstream>
#include <sstream>

using JSONValue = Lemon::JSONValue;

void PrintObject(JSONValue& json);

void PrintArray(JSONValue& json){
    std::cout << "Array[ ";
    for(auto& v : *json.data.array){
        if(v.IsObject()){
            PrintObject(v);
        } else if(v.IsArray()){
            PrintArray(v);
        } else if(v.IsFloat()){
            std::cout << v.AsFloat();
        } else if(v.IsString()){
            std::cout << v.AsString();
        } else if(v.IsNumber()){
            std::cout << v.AsSignedNumber();
        } else if(v.IsBool()){
            std::cout << (v.AsBool() ? "true" : "false");
        } else {
            std::cout << "null";
        }
        std::cout << ", ";
    }
    std::cout << " ]\n";
}

void PrintObject(JSONValue& json){
    std::cout << "Object{ ";
    for(auto& v : *json.data.object){
        std::cout << "key: \"" << v.first << "\", value: ";
        if(v.second.IsObject()){
            PrintObject(v.second);
        } else if(v.second.IsArray()){
            PrintArray(v.second);
        } else if(v.second.IsFloat()){
            std::cout << v.second.AsFloat();
        } else if(v.second.IsString()){
            std::cout << v.second.AsString();
        } else if(v.second.IsNumber()){
            std::cout << v.second.AsSignedNumber();
        } else if(v.second.IsBool()){
            std::cout << (v.second.AsBool() ? "true" : "false");
        } else {
            std::cout << "null";
        }
        std::cout << ",\n";
    }
    std::cout << " }\n";
}

std::stringstream ss;
std::ifstream file;
std::string str;
int main(int argc, char** argv){
    if(argc < 2){
        std::cout << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }

    file.open(argv[1]);

    if(!file.is_open()){
        std::cout << "Error opening file " << argv[1] << "!\n";
        return 2;
    }

    ss << file.rdbuf();

    str = ss.str();
    std::string_view sv = std::string_view(str);
    Lemon::JSONParser p = Lemon::JSONParser(sv);

    auto json = p.Parse();
    if(!json.IsObject()){
        std::cout << "Error parsing JSON file!\n";
        return 3;
    }

    PrintObject(json);

    return 0;
}