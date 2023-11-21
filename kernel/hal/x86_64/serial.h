#pragma once

namespace serial {

void init();
void debug_write_char(char c);
void debug_write_string(const char *str);

}
