#pragma once

void KernelAssertionFailed(const char* msg, const char* file, int line);
#define assert(expr) (void)((expr) || (KernelAssertionFailed(#expr, __FILE__, __LINE__), 0))