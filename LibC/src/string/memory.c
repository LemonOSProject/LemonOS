#include <stddef.h>
#include <stdint.h>

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
		dp[i] = sp[i];
		count--;
	} 
	return dest;
}

void* memchr (const void* src, int c, size_t count){
	const char* ptr = (const char*)src;
	for(size_t i = 0; i < count; i++, ptr++){
		if(*ptr == c){ // Found first instance of c
			return ptr; // Return pointer to first instance of c
		}
	}
}

int memcmp(const void* ptr1, const void* ptr2, size_t count){
	const char* p1 = (const char*)ptr1;
	const char* p2 = (const char*)ptr2;
	for(size_t i = 0; i < count; i++, p1++, p2++){
		if(*p1 != *p2){
			return (*p1 > *p2) - (*p1 < *p2); // Value >0 if p1 > p2, 0 if all equal and <0 if p1 < p2.
		}
	}
	return 0;
}

void *memmove(void* dest, void* src, size_t count) {
	char* buffer = malloc(count);

	memcpy(buffer, src, count);
	memcpy(dest, buffer, count);
	
	free(buffer);
	return dest;
}