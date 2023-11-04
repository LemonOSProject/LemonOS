unsigned long int rand_next = 1;

unsigned int rand() {
    rand_next = rand_next * 1103515245 + 12345;
    return ((unsigned int)(rand_next / 65536) % 32768);
}

int abs(int num) { return num < 0 ? -num : num; }