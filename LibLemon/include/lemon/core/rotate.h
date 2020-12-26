#pragma once

#include <stddef.h>

template<typename I>
constexpr static inline I rotateRight(I value, size_t amount){
	return (value >> amount) | (value << (-amount & (sizeof(I) * 8 - 1)));
}

template<typename I>
constexpr static inline I rotateLeft(I value, size_t amount){
	return (value << amount) | (value >> (-amount & (sizeof(I) * 8 - 1)));
}