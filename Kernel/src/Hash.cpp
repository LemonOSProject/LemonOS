#include <Hash.h>

#include <StringView.h>
#include <String.h>

template<>
unsigned Hash<unsigned long>(const unsigned long& value){
	return HashU(value);
}

template<>
unsigned Hash<unsigned int>(const unsigned int& value){
	return HashU(value);
}

template<>
unsigned Hash<unsigned short>(const unsigned short& value){
	return HashU(value);
}

template<>
unsigned Hash<unsigned char>(const unsigned char& value){
	return HashU(value);
}

template<>
unsigned Hash<StringView>(const StringView& sv){
    const char* str = sv.Data();
    unsigned value = (*str << 1);

    while(char c = *str++){
        value ^= HashU(c);
    }

    return value;
}

template<>
unsigned Hash<String>(const String& s) {
    const char* str = s.c_str();
    unsigned value = (*str << 1);

    while(char c = *str++){
        value ^= HashU(c);
    }

    return value;
}
