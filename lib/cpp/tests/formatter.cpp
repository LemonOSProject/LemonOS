#include "../include/le/formatter.h"

#include <stdio.h>
#include <string.h>

template<typename T>
void test(T t) {}

int main() {
    char buffer[4096];

    format_n(buffer, 4096, "fmt test char: {}, string: {}!", 'c', "test");
    puts(buffer);
    //assert(!strcmp(buffer, "fmt test char: c, string: test!"));

    format_n(buffer, 20, "fmt test char: {}, string: {}!", 'c', "test");
    puts(buffer);

    format_n(buffer, 4096, "fmt test int: {}, ptr: {}", 123, (void*)0xdeadbeef);
    puts(buffer);
    //assert(!strcmp(buffer, "fmt test int: 123, ptr: 0xdeadbeef"));

    return 0;
}
