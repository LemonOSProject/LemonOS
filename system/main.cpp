void _start() {
    asm volatile(
        "xor %rbp, %rbp\n"
        "call main\n"
        "ud2\n"
    );
}

extern "C"
int main() {
    while (1);
}
