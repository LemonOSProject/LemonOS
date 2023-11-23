#include "../include/formatter.h"

#include <stdio.h>

int main() {
    char buffer[4096];

    format_n(buffer, 4096, "fmt test char: {}, string: {}!", 'c', "test");
    puts(buffer);

    format_n(buffer, 20, "fmt test char: {}, string: {}!", 'c', "test");
    puts(buffer);

    return 0;
}
