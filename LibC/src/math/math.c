unsigned long int rand_next = 1;

unsigned int rand()
{
	rand_next = rand_next * 1103515245 + 12345;
	return ((unsigned int)(rand_next / 65536) % 32768);
}

unsigned int srand(unsigned int seed)
{
	seed = seed * 1103515245 + 12345;
	return ((unsigned int)(seed / 65536) % 32768);
}


double floor(double num) {
	int x = (int)num;
	return num < x ? x - 1 : x;
}

int abs(int num) {
	return num < 0 ? -num : num;
}