#pragma once

#include <fstream>
#include <nlohmann/json.hpp>

namespace Lemon{
    class JSONConfig{
    protected:
        nlohmann::json j;

    public:
        JSONConfig();

        template<typename T>
        T& GetItem(std::string path){

        }

        bool IsArray(std::string path){
            
        }

        bool IsNumber(std::string path){

        }

        bool IsString(std::string path){

        }

        bool IsBoolean(std::string path){
            
        }
    };
}