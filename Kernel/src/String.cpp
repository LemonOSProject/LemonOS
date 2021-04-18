#include <String.h>

#include <stdint.h>
#include <Liballoc.h>
#include <Paging.h>

int HexStringToPointer(const char* buffer, size_t bufferSize, uintptr_t& pointerValue){
	size_t n = 0;

	pointerValue = 0;
	while(*buffer && n++ < bufferSize){
		char c = *buffer++;

		pointerValue <<= 4; // Shift 4-bits for next character
		if(c >= '0' && c <= '9'){
			pointerValue |= (c - '0') & 0xf;
		} else if(c >= 'a' && c <= 'f'){
			pointerValue |= (c - 'a' + 0xa) & 0xf;
		} else if(c >= 'A' && c <= 'F'){
			pointerValue |= (c - 'A' + 0xa) & 0xf;
		} else {
			return 1; // Invalid character
		}
	}

	return 0;
}

void reverse(char* str, size_t length) {
	char* end = str + length - 1;

	for (size_t i = 0; i < length / 2; i++)
	{
		char c = *end;
		*end   = *str;
		*str = c;

		str++;
		end--;
	}
}

char* itoa(unsigned long long num, char* str, int base) {
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

extern "C"
void* memset(void* src, int c, size_t count) {
	uint8_t* xs = (uint8_t*)src;

	while (count--)
		*xs++ = c;
		
	return src;
}

extern "C"
void *memcpy(void* dest, const void* src, size_t count) {
	const char *sp = (char*)src;
	char *dp = (char *)dest;
	for(size_t i = count; i >= sizeof(uint64_t); i = count){
		*((uint64_t*)dp) = *((uint64_t*)sp);
		sp = sp + sizeof(uint64_t);
		dp = dp + sizeof(uint64_t);
		count -= sizeof(uint64_t);
	}

	for(size_t i = count; i >= 4; i = count){
		*((uint32_t*)dp) = *((uint32_t*)sp);
		sp = sp + 4;
		dp = dp + 4;
		count -= 4;
	}

	for (size_t i = count; i > 0; i = count){
		*(dp++) = *(sp++);
		count--;
	} 

	return dest;
}

extern "C"
int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t* a = (uint8_t*)s1;
    const uint8_t* b = (uint8_t*)s2;

    for (size_t i = 0; i < n; i++) {
        if (a[i] < b[i]) {
            return -1;
        } else if (a[i] > b[i]) {
            return 1;
        }
    }

    return 0;
}

void strcpy(char* dest, const char* src)
{
	while(*src){
		*(dest++) = *(src++);
	}
	*dest = 0;
}

void strncpy(char* dest, const char* src, size_t n)
{
	while(n-- && *src){
		*(dest++) = *(src++);
	}
	*dest = 0;
}

size_t strlen(const char* str)
{
	size_t i = 0;
	while(str[i]) i++;
	return i;
}

int strcmp(const char* s1, const char* s2)
{
	while(*s1 == *s2)
	{
		if(!*(s1++)){
			return 0; // Null terminator
		}

		s2++;
	}
	return (*s1) - *(s2);
}

int strncmp(const char* s1, const char* s2, size_t n)
{
	for (size_t i = 0; i < n; i++)
	{
		if (s1[i] != s2[i]) {
			return s1[i] < s2[i] ? -1 : 1;
		} else if (s1[i] == '\0') {
			return 0;
		}
	}

	return 0;
}

// strchr - Get pointer to first occurance of c in string s
char* strchr(const char *s, int c)
{
	while (*s != (char)c)
		if (!*s++)
			return nullptr;
	return (char*)s;
}

// strnchr - Get pointer to first occurance of c in string s, searching at most n characters
char* strnchr(const char *s, int c, size_t n)
{
	while (n-- && *s != (char)c)
		if (!*s++)
			return nullptr;

	if(n <= 0){
		return nullptr;
	}
	
	return const_cast<char*>(s);
}

// strrchr - Get pointer to last occurance of c in string s
char* strrchr(const char *s, int c)
{
	const char* occ = nullptr;
	while (*s)
		if (*s++ == c)
			occ = s;
	return const_cast<char*>(occ);
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
char *strtok(char * str, const char * delim) {
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

// strtok - reentrant strtok
char *strtok_r(char * str, const char* delim, char** saveptr) {
	char*& p = *saveptr;
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
	return dest;
}

char* strncat(char* dest, const char* src, size_t n){
    strncpy(dest + strlen(dest), src, n);
	return dest;
}

char toupper(char c){
	if(c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	else return c;
}

char* strupr(char* s){
	for(size_t i = 0; s[i] != '\0'; i++){
		s[i] = toupper(s[i]);
	}
	return s;
}

char* strnupr(char* s, size_t n){

	for(size_t i = 0; s[i] && i < n; i++){
		if(!(i < n)) break;
		s[i] = toupper(s[i]);
	}

	return s;
}

char* strdup(const char* s){
	char* buf = new char[strlen(s) + 1];

	strcpy(buf, s);

	return buf;
}