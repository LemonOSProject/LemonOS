#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdio.h>

class CFGParser{
	struct CFGItem{
		std::string name;
		std::string value;
	};
private:
	std::vector<std::pair<std::string, std::vector<CFGItem>>> items;
	FILE* cfgFile = nullptr;
public:
	CFGParser(const char* path);
	~CFGParser();

	void Parse();
	auto& GetItems() { return items; };
};