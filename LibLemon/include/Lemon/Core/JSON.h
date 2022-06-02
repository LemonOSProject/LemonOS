#pragma once

#include <cassert>
#include <map>
#include <string>
#include <vector>

#include <Lemon/Core/Lexer.h>

namespace Lemon {
using JSONKey = std::string;

struct JSONValue {
    enum {
        TypeString,
        TypeArray,
        TypeObject,
        TypeNumber,
        TypeBoolean,
        TypeNull,
    };

    int type;

    struct {
        bool isFloatingPoint = false;
        bool isSigned = true;
    } traits;

    union {
        std::string* str = nullptr;
        std::vector<JSONValue>* array;
        std::map<JSONKey, JSONValue>* object;
        unsigned long uLong;
        long sLong;
        float fl;
        double dbl;
        bool boolean;
    } data;

    JSONValue() { type = TypeNull; }

    JSONValue(const JSONValue& v) = delete;
    JSONValue(JSONValue&& v) {
        type = v.type;
        traits = v.traits;
        
        data = v.data;

        v.type = TypeNull;
        v.data.uLong = 0;
    }

    JSONValue(const std::string& s) {
        type = TypeString;

        data.str = new std::string(s);
    }

    JSONValue(std::string&& s) {
        type = TypeString;

        data.str = new std::string(s);
    }

    JSONValue(std::vector<JSONValue>&& v) {
        type = TypeArray;

        data.array = new std::vector<JSONValue>(std::move(v));
    }

    JSONValue(std::map<JSONKey, JSONValue>&& o) {
        type = TypeObject;

        data.object = new std::map<JSONKey, JSONValue>(std::move(o));
    }

    static JSONValue NewObject() {
        JSONValue v;
        v.type = TypeObject;
        v.data.object = new std::map<JSONKey, JSONValue>();

        return v;
    }

    JSONValue(unsigned long ul) {
        type = TypeNumber;

        data.uLong = ul;
        traits.isSigned = false;
        traits.isFloatingPoint = false;
    }

    JSONValue(long l) {
        type = TypeNumber;

        data.sLong = l;
        traits.isSigned = true;
        traits.isFloatingPoint = false;
    }

    JSONValue(double f) {
        type = TypeNumber;

        data.dbl = f;
        traits.isFloatingPoint = true;
    }

    JSONValue(bool b) {
        type = TypeBoolean;

        data.boolean = b;
    }

    ~JSONValue() {
        ReleaseData();
    }

    JSONValue& operator=(JSONValue&& other) {
        ReleaseData();

        type = other.type;
        traits = other.traits;
        data = other.data;

        other.type = TypeNull;
        other.data.uLong = 0;

        return *this;
    }

    void ReleaseData() {
        if(type == TypeString) {
            delete data.str;
        } else if(type == TypeArray) {
            delete data.array;
        } else if(type == TypeObject) {
            delete data.object;
        }
    }

    inline JSONValue& operator[](const std::string& s) {
        assert(type == TypeObject && data.object);

        return data.object->at(s);
    }

    inline bool IsString() { return type == TypeString; }

    inline bool IsNumber() { return type == TypeNumber; }
    inline bool IsFloat() { return traits.isFloatingPoint; }
    inline bool IsSigned() { return traits.isSigned; }

    inline bool IsBool() { return type == TypeBoolean; }

    inline bool IsArray() { return type == TypeArray; }

    inline bool IsObject() { return type == TypeObject; }

    inline bool IsNull() { return type == TypeNull; }

    std::string AsString() { return *data.str; }

    template <typename I = long> inline I AsSignedNumber() { 
        assert(!IsFloat());
        return static_cast<I>(data.sLong); }

    template <typename I = unsigned long> inline I AsUnsignedNumber() { return static_cast<I>(data.uLong); }

    template <typename I = double> inline I AsFloat() {
        if(traits.isFloatingPoint) {
            return static_cast<I>(data.dbl);
        } else {
            return static_cast<I>(data.sLong);
        }
    }

    inline bool AsBool() { return data.boolean; }
};

class JSONParser : protected BasicLexer {
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

int WriteJSON(const char* file, JSONValue& object);

} // namespace Lemon
