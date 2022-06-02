#include <Lemon/Core/JSON.h>

#include <Lemon/Core/Format.h>
#include <Lemon/Core/Logger.h>

#include <stdio.h>

//#define LIBLEMON_DEBUG_JSON 1

namespace Lemon {
JSONParser::JSONParser(std::string_view& v) : BasicLexer(v) {}

JSONParser::JSONParser(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("[LibLemon] Warning: Failed to open JSON file '%s' for reading!\n", path);
        return;
    }

    fseek(file, 0, SEEK_END);

    size_t len = ftell(file);

    if (len <= 0x1000) {
        buffer.resize(len);

        fseek(file, 0, SEEK_SET);

        fread(buffer.data(), 1, len, file);
    } else {
        fseek(file, 0, SEEK_SET);

        char buf[0x1000];
        while (size_t sz = fread(buf, 0x1000, 1, file)) {
            buffer.insert(buffer.end(), buf, buf + sz);
        }
    }

    buffer.push_back(0);
    fclose(file);

    sv = std::string_view(buffer.data());
    it = sv.begin();
}

JSONValue JSONParser::Parse() {
    JSONValue v = JSONValue();

    if (sv.empty()) {
        return v;
    }
    EatWhitespace();

    return ParseObject();
}

std::string JSONParser::ParseString() {
    if (!EatOne('"')) {
        return "";
    }

    std::string str;
    char c;
    while (!End()) {
        c = Eat();

        if (c == '"') {
            return str;
        }

        if (End()) {
            return str; // Unterminated string
        }

        if (c == '\\') { // Control character
            c = Eat();
            switch (c) {
            case '"':
                str += '"';
                break;
            case '\\':
                str += '\\';
                break;
            case '/':
                str += '/';
                break;
            case 'b':
                str += '\b';
                break;
            case 'f':
                str += '\f';
                break;
            case 'n':
                str += '\n';
                break;
            case 'r':
                str += '\r';
                break;
            case 't':
                str += '\t';
                break;
            default: // Invalid escape character
#ifdef LIBLEMON_DEBUG_JSON
                printf("Invalid escape sequence '\\%c', line %d.\n", c, line);
#endif
                return "";
            }
        } else {
            str += c;
        }
    }

    return "";
}

int JSONParser::ParseValue(JSONValue& val) {
    EatWhitespace();

    char c = Peek();
    JSONValue& v = val;

    if (isdigit(c) || c == '-') {
        bool negative = false;
        if (c == '-') {
            negative = true;
            Eat();
            c = Peek();
        }

        if (c == '0' && Peek(1) != '.') { // Number, Leading zeros are not allowed unless value == 0 or is a decimal
            v = JSONValue(0L);

            Eat();
        } else {
            std::string_view num = EatWhile([](char c) -> bool { return isdigit(c) || c == '.'; });

            if (size_t pt = num.find('.'); pt != num.npos) { // Check for a decimal point
                if (num.find('.', pt + 1) != num.npos) {
#ifdef LIBLEMON_DEBUG_JSON
                    printf("Expected no more than one '.', line %d.\n", line);
#endif
                    return 1; // We can't have more than one decimal point
                }

                if (!negative) {
                    v = JSONValue(std::stof(std::string(num)));
                } else {
                    v = JSONValue(-std::stof(std::string(num)));
                }
            } else {

                if (!negative) {
                    v = JSONValue(std::stoul(std::string(num)));
                } else {
                    v = JSONValue(-std::stol(std::string(num)));
                }
            }
        }
    } else if (c == '"') { // String
        v = JSONValue(ParseString());
    } else if (c == '{') { // Object
        v = ParseObject();

        if (v.IsNull()) {
            return 1; // Should not be null
        }
    } else if (c == '[') { // Array
        v = ParseArray();

        if (v.IsNull()) {
            return 1; // Should not be null
        }
    } else if (c == 't') {
        if (!EatWord("true")) {
            return 1; // Unknown word
        }

        v = JSONValue(true);
    } else if (c == 'f') {
        if (!EatWord("false")) {
            return 1; // Unknown word
        }

        v = JSONValue(false);
    } else if (c == 'n') {
        if (!EatWord("null")) {
            return 1; // Unknown word
        }

        v = JSONValue(); // One of the only times there should be null
    } else {
#ifdef LIBLEMON_DEBUG_JSON
        printf("Unknown leading value character '%c', line %d.\n", c, line);
#endif
    }

    return 0;
}

JSONValue JSONParser::ParseObject() {
    EatWhitespace();

    if (!EatOne('{')) {
#ifdef LIBLEMON_DEBUG_JSON
        printf("Expected {, line %d.\n", line);
#endif
        return JSONValue();
    }

    std::map<JSONKey, JSONValue> values;
    while (!End()) {
        EatWhitespace();

        std::string key = ParseString();
        if (!key.length()) {
#ifdef LIBLEMON_DEBUG_JSON
            printf("Key is empty, line %d.\n", line);
#endif
            return JSONValue();
        }

        EatWhitespace();

        if (!EatOne(':')) {
#ifdef LIBLEMON_DEBUG_JSON
            printf("Expected :, line %d.\n", line);
#endif
            return JSONValue();
        }

        JSONValue v;
        if (ParseValue(v)) {
            return JSONValue(); // Error parsing value
        }

        values[key] = std::move(v);

        EatWhitespace();

        char c = Peek();
        if (c != ',') { // If there is a comma keep going, otherwise break
#ifdef LIBLEMON_DEBUG_JSON
            printf("End of object '%c', line %d.\n", c, line);
#endif
            break;
        } else {
            Eat();
        }
    }

    EatWhitespace();
    if (!EatOne('}')) {
#ifdef LIBLEMON_DEBUG_JSON
        printf("Expected }, line %d.\n", line);
#endif
        return JSONValue();
    }

    return JSONValue(std::move(values));
}

JSONValue JSONParser::ParseArray() {
    if (!EatOne('[')) {
#ifdef LIBLEMON_DEBUG_JSON
        printf("Expected [, line %d.\n", line);
#endif
        return JSONValue();
    }

    std::vector<JSONValue> values;
    while (!End()) {
        EatWhitespace();

        JSONValue v;
        if (ParseValue(v)) {
            return JSONValue(); // Error parsing value
        }

        values.push_back(std::move(v));

        EatWhitespace();

        char c = Peek();
        if (c != ',') { // If there is a comma keep going, otherwise break
            break;
        } else {
            Eat();
        }
    }

    EatWhitespace();
    if (!EatOne(']')) {
#ifdef LIBLEMON_DEBUG_JSON
        printf("Expected ], line %d.\n", line);
#endif
        return JSONValue();
    }

    return JSONValue(std::move(values));
}


static void IndentLine(FILE* f, int indent) {
    while(indent--) {
        fputs("    ", f);
    }
};

static int EmitValue(FILE* file, JSONValue* val, int indent);
static int EmitArray(FILE* file, JSONValue* array, int indent) {
    fputs("[\n", file);

    auto& v = *array->data.array;
    for(unsigned i = 0; i < v.size(); i++) {
        IndentLine(file, indent + 1);
        EmitValue(file, &v.at(i), indent + 1);

        if(i != v.size() - 1) {
            // Place a comma if not last value in array
            fputc(',', file);
        }
        fputc('\n', file);
    }

    IndentLine(file, indent);
    fputc(']', file);

    return 0;
}

static int EmitObject(FILE* file, JSONValue* object, int indent) {
    fputs("{\n", file);

    for(std::pair<const JSONKey, JSONValue>& v : *object->data.object) {
        IndentLine(file, indent + 1);
        fmt::print(file, "\"{}\" : ", v.first);
        EmitValue(file, object, indent + 1);
        fputs(",\n", file);
    }

    fputc('}', file);
    return 0;
};

static int EmitValue(FILE* file, JSONValue* val, int indent) {
    if(val->IsNull()) {
        fputs("null", file);
    } else if(val->IsNumber()) {
        if(val->IsFloat()) {
            fmt::print(file, "{}", val->AsFloat());
        } else if(val->IsSigned()) {
            fmt::print(file, "{}", val->AsSignedNumber());
        } else {
            fmt::print(file, "{}", val->AsUnsignedNumber());
        }
    } else if(val->IsBool()) {
        if(val->AsBool()) {
            fputs("true", file);
        } else {
            fputs("false", file);
        }
    } else if(val->IsString()) {
        fmt::print(file, "{}", val->AsString());
    } else if(val->IsArray()) {
        EmitArray(file, val, indent);
    } else if(val->IsObject()) {
        EmitObject(file, val, indent);
    } else {
        Logger::Error("Invalid JSON type {}!", val->type);
        return 1;
    }

    return 0;
};

int WriteJSON(const char* filepath, JSONValue& object) {
    assert(object.IsObject());

    // Truncate the file
    FILE* file = fopen(filepath, "w+");
    if(!file) {
        Logger::Error("WriteJSON: Failed to open {} for writing.", filepath);
        return 1;
    }

    int r = EmitObject(file, &object, 0);
    fclose(file);
    return r;
}
} // namespace Lemon
