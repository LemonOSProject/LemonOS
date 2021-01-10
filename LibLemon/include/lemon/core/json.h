#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cassert>

#include <lemon/core/lexer.h>

namespace Lemon{
    using JSONKey = std::string;

    struct JSONValue{
        enum {
            TypeString,
            TypeArray,
            TypeObject,
            TypeNumber,
            TypeBoolean,
            TypeNull,
        };

        int type;
        bool isFloatingPoint = false;
        bool isSigned = true;

        union{
            std::string* str = nullptr;
            std::vector<JSONValue>* array;
            std::map<JSONKey, JSONValue>* object;
            unsigned long uLong;
            long sLong;
            float fl;
            double dbl;
            bool boolean;
        };

        JSONValue(){
            type = TypeNull;
        }

        JSONValue(const std::string& s){
            type = TypeString;

            str = new std::string(s);
        }

        JSONValue(std::string&& s){
            type = TypeString;

            str = new std::string(s);
        }

        JSONValue(const std::vector<JSONValue>& v){
            type = TypeArray;

            array = new std::vector<JSONValue>(v);
        }

        JSONValue(std::vector<JSONValue>&& v){
            type = TypeArray;

            array = new std::vector<JSONValue>(v);
        }

        JSONValue(const std::map<JSONKey, JSONValue>& o){
            type = TypeObject;

            object = new std::map<JSONKey, JSONValue>(o);
        }

        JSONValue(std::map<JSONKey, JSONValue>&& o){
            type = TypeObject;

            object = new std::map<JSONKey, JSONValue>(o);
        }

        JSONValue(unsigned long ul){
            type = TypeNumber;

            uLong = ul;
            isSigned = false;
        }

        JSONValue(long l){
            type = TypeNumber;

            sLong = l;
            isSigned = true;
        }

        JSONValue(float f){
            type = TypeNumber;

            fl = f;
            isFloatingPoint = true;
        }

        JSONValue(bool b){
            type = TypeBoolean;

            boolean = b;
        }

        inline const JSONValue& operator[](const std::string& s){
            assert(type == TypeObject && object);

            return object->at(s);
        }

        inline bool IsString(){
            return type == TypeString;
        }

        inline bool IsNumber(){
            return type == TypeNumber;
        }

        inline bool IsBool(){
            return type == TypeBoolean;
        }

        inline bool IsArray(){
            return type == TypeArray;
        }

        inline bool IsObject(){
            return type == TypeObject;
        }

        inline bool IsNull(){
            return type == TypeNull;
        }

        std::string AsString(){
            return *str;
        }

        template<typename I = long>
        inline I AsSignedNumber(){
            return static_cast<I>(sLong);
        }

        template<typename I = unsigned long>
        inline I AsUnsignedNumber(){
            return static_cast<I>(uLong);
        }

        template<typename I = float>
        inline I AsFloat(){
            return static_cast<I>(fl);
        }

        inline bool AsBool(){
            return boolean;
        }
    };

    class JSONParser : protected BasicLexer{
    protected:
        std::vector<char> buffer;

        std::string ParseString();

        int ParseValue(JSONValue& val);

        JSONValue ParseObject();
        JSONValue ParseArray();

    public:
        JSONParser(std::string_view& v);
        JSONParser(const char* path);

        JSONValue Parse();
    };
}