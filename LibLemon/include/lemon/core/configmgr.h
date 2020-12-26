#pragma once

#include <string>
#include <map>
#include <vector>

namespace Lemon{
    using ConfigKey = std::string;

    class ConfigTypeException : public std::exception{
        const char* what() const noexcept {
            return "Error accessing ConfigurationValue: Invalid type.";
        }
    };

    class ConfigNullException : public std::exception{
        const char* what() const noexcept {
            return "Error accessing ConfigurationValue: Null object.";
        }
    };

    struct ConfigurationValue{
        enum ConfigurationValueType {
            TypeString,
            TypeArray,
            TypeObject,
            TypeUnsigned,
            TypeSigned,
            TypeBoolean,
            TypeNull,
        };

        ConfigurationValueType type;

        union{
            std::string* string = nullptr;
            std::vector<ConfigurationValue>* array;
            std::map<ConfigKey, ConfigurationValue>* object;
            unsigned long uLong;
            long sLong;
            bool boolean;
        };

        ConfigurationValue(){
            type = TypeNull;
        }

        ConfigurationValue(const std::string& s){
            type = TypeString;

            string = new std::string(s);
        }

        ConfigurationValue(std::string&& s){
            type = TypeString;

            string = new std::string(s);
        }

        ConfigurationValue(const std::vector<ConfigurationValue>& v){
            type = TypeArray;

            array = new std::vector<ConfigurationValue>(v);
        }

        ConfigurationValue(std::vector<ConfigurationValue>&& v){
            type = TypeArray;

            array = new std::vector<ConfigurationValue>(v);
        }

        ConfigurationValue(const std::map<ConfigKey, ConfigurationValue>& o){
            type = TypeObject;

            object = new std::map<ConfigKey, ConfigurationValue>(o);
        }

        ConfigurationValue(std::map<ConfigKey, ConfigurationValue>&& o){
            type = TypeObject;

            object = new std::map<ConfigKey, ConfigurationValue>(o);
        }

        ConfigurationValue(unsigned long ul){
            type = TypeUnsigned;

            uLong = ul;
        }

        ConfigurationValue(long l){
            type = TypeSigned;

            sLong = l;
        }

        ConfigurationValue(bool b){
            type = TypeBoolean;

            boolean = b;
        }

        inline ConfigurationValue& operator[](const std::string& s){
            if(type != TypeObject){
                throw new ConfigTypeException();
            }

            if(!object){
                throw new ConfigNullException();
            }

            return object->at(s);
        }
    };

    class ConfigurationParser{
    private:
        ConfigurationValue root;
    public:
        virtual void Parse() = 0;
        ConfigurationValue& Find(const std::string& path);

        virtual ~ConfigurationParser() = default;
    };
}