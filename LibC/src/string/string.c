#include <stddef.h>

int strlen(const char* str)
{
	int i = 0;
	while(str[i] != '\0') i++;
	return i;
}

char* strcpy(char* dest, const char* src)
{
	int i = 0;
	for(i = 0; i < strlen(src); i++)
	{
		dest[i] = src[i];
	}
	dest[i] = '\0';
	return dest;
}

void strncpy(char* dest, const char* src, size_t n)
{
	int i;
	for(i = 0; i < n; i++)
	{
		dest[i] = src[i];
	}
	dest[i] = '\0';
}

char* strcat(char* dest, const char* src){
    strcpy(dest + strlen(dest), src);
	return dest;
}

char *strchr(const char *s, int c)
{
	while (*s != (char)c)
		if (!*s++)
			return 0;
	return (char *)s;
}

char *strrchr(const char *s, int c)
{
	char* str = 0;
	while(*s){
		if(*s == (char)c)
			str = (char*)s;
		if (!*s++)
			return str;
	}

	return (char *)s;
}

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

int strcmp(const char *str1, const char *str2)
{
	for (; *str1==*str2 && *str1 != '\0'; str1++, str2++);
	return *(unsigned char *)str1 - *(unsigned char *)str2;
}

int strncmp(const char *str1, const char *str2, size_t n)
{
	for (int i = 0; *str1==*str2 && *str1 != '\0' && i < n; i++, str1++, str2++);
	return *(unsigned char *)str1 - *(unsigned char *)str2;
}

int strcasecmp(const char *str1, const char *str2)
{
	for (; tolower(*str1)==tolower(*str2) && *str1 != '\0'; str1++, str2++);
	return *(unsigned char *)str1 - *(unsigned char *)str2;
} 

int strncasecmp(const char *str1, const char *str2, size_t n)
{
	for (int i = 0; tolower(*str1)==tolower(*str2) && *str1 != '\0' && i < n; i++, str1++, str2++);
	return *(unsigned char *)str1 - *(unsigned char *)str2;
}

char* strdup(char* str){
	char* ret = (char*)malloc(strlen(str)+1);
	memset(ret, 0, strlen(str) + 1);
	strcpy(ret, str);
}