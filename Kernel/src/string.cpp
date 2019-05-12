#include <string.h>

#include <stdint.h>

void reverse(char *str, int length)
{
   int c;
   char *begin, *end, temp;

   begin  = str;
   end    = str;

   for (c = 0; c < length - 1; c++)
      end++;

   for (c = 0; c < length/2; c++)
   {
      temp   = *end;
      *end   = *begin;
      *begin = temp;

      begin++;
      end--;
   }
}

char* itoa(unsigned long long num, char* str, int base)
{
	int i = 0;

	if (num == 0)
	{
		str[i++] = '0';
		str[i] = '\0';
		return str;
	}

	while (num != 0)
	{
		int rem = num % base;
		str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
		num = num / base;
	}

	str[i] = '\0';

	reverse(str, i);

	return str;
}

void* memset(void* src, int c, size_t count)
{
	unsigned char *xs = (uint8_t*)src;

	while (count--)
		*xs++ = c;
	return src;
}

void *memcpy(void* dest, void* src, size_t count) {
	const char *sp = (char *)src;
	char *dp = (char *)dest;
	for(size_t i = count; i >= 4; i = count){
		*((uint32_t*)dp) = *((uint32_t*)sp);
		sp = sp + 4;
		dp = dp + 4;
		count -= 4;
	}
	for (size_t i = count; i > 0; i = count){
		dp[i] = sp[i];
		count--;
	} 
	return dest;
}

void strcpy(char* dest, const char* src)
{
	for(int i = 0; src[i] != '\0'; i++)
	{
		dest[i] = src[i];
	}
}

int strlen(char* str)
{
	int i = 0;
	while(str[i] != '\0') i++;
	return i;
}

int strcmp(char* s1, char* s2)
{
	for (int i = 0; ; i++)
	{
		if (s1[i] != s2[i])
			return s1[i] < s2[i] ? -1 : 1;
		else if (s1[i] == '\0')
			return 0;
	}
}

// strchr - Get pointer to first occurance of c in string s
char *strchr(const char *s, int c)
{
	while (*s != (char)c)
		if (!*s++)
			return 0;
	return (char *)s;
}

// strspn - Get initial length of s1 including only the characters of s2
size_t strspn(const char *s1, const char *s2)
{
	size_t ret = 0;
	while (*s1 && strchr(s2, *s1++))
		ret++;
	return ret;
}

// strspn - Get initial length of s1 excluding the characters of s2
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

// strtok - breaks str into tokens using specified delimiters
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

char* strcat(char* dest, const char* src){
    strcpy(dest + strlen(dest), src);
}