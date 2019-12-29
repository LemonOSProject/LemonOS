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