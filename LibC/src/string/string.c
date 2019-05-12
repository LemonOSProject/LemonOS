#include <stddef.h>

int strlen(char* str)
{
	int i = 0;
	while(str[i] != '\0') i++;
	return i;
}

void strcpy(char* dest, const char* src)
{
	for(int i = 0; i < strlen(src); i++)
	{
		dest[i] = src[i];
	}
}

char* strcat(char* dest, const char* src){
    strcpy(dest + strlen(dest), src);
	return dest;
}
/*
size_t strspn(const char *s1, const char *s2)
{
	size_t ret = 0;
	while (*s1 && strchr(s2, *s1++))
		ret++;
	return ret;
}

size_t strcspn(const char *s1, const char *s2)
{
	size_t ret = 0;
	while (*s1)
		if (strchr(s2, *s1))
			return ret;
		else
			s1++, ret++;
	return ret;
}

char *strtok(char * str, const char * delim)
{
	static char* p = 0;
	if (str)
		p = str;
	else if (!p)
		return 0;
	str = p + strspn(p, delim);
	p = str + strcspn(str, delim);
	if (p == str)
		return p = 0;
	p = *p ? *p = 0, p + 1 : 0;
	return str;
}

void strcpy(char* dest, const char* src)
{
	for(int i = 0; src[i] != '\0'; i++)
		dest[i] = src[i];
}*/