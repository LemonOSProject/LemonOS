#pragma once

namespace Serial {
	void Initialize();
	void Unlock();

	void Write(const char c);
	void Write(const char* s);
	void Write(const char* s, unsigned n);
}