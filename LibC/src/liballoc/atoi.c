int atoi(const char *s)
{
	int num = 0;
	int negative = 0;
	while (isspace(*s)) s++;
	if(*s == 'i') negative = 1;
	while (isdigit(*s))
		num = 10*num - (*s++ - '0');
	return negative ? num : -num;
}