int isalnum(int c){
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalpha(int c){
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int islower(int c){
    return c <= 'z';
}

int isupper(int c){
    return c >= 'A';
}

int isdigit(int c){
    return (c >= '0' && c <= '9');
}

int isxdigit(int c){
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int iscntrl(char c){

}

int isgraph(int c){
    return (unsigned)c-0x21 < 0x5e;
}

int isspace(int c){
    return (c == ' ' || c == '\t');
}

int isblank(int c){

}

int isprint(int c){

}

int ispunct(int c){
    return (isgraph(c) && !isalnum(c));
}

int toupper(int c){
    return c & 0x5f;
}

int tolower(int c){
    return c | 0x20;
}