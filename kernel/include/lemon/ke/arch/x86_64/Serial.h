#pragma once

namespace Serial {
	void Initialize();
	void Unlock();

	void write(const char c);
	void write(const char* s);
	void write(const char* s, unsigned n);
}