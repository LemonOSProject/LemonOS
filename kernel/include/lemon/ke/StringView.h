#pragma once

#include <Assert.h>
#include <String.h>

class StringView {
public:
    StringView(const char* _string)  : string(_string) {
        length = strlen(string);
    }

    inline unsigned Length() const { return length; }
    inline const char* Data() const { return string; } 

    inline char operator[](unsigned pos) const {
        assert(pos < length);
        return string[pos];
    }
private:
    unsigned length;
    const char* string;
};

inline bool operator==(const StringView& l, const StringView& r){
    return !(strcmp(l.Data(), r.Data()));
}

inline bool operator!=(const StringView& l, const StringView& r){
    return strcmp(l.Data(), r.Data());
}