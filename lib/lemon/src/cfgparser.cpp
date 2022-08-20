#include <Lemon/Core/CFGParser.h>

enum ParserState {
    ParserStateHeading,
    ParserStateName,
    ParserStateValue,
};

CFGParser::CFGParser(const char* path) {
    cfgFile = fopen(path, "r");

    if (!cfgFile) {
        printf("CFGParser: Failed to open %s!\n", path);
        return;
    }

    fseek(cfgFile, 0, SEEK_END);

    size_t cfgSize = ftell(cfgFile);

    if (cfgSize <= 0x8000) {
        cfgData.resize(cfgSize);

        fseek(cfgFile, 0, SEEK_SET);

        fread(cfgData.data(), cfgSize, 1, cfgFile);
    } else {
        char buf[512];
        while (fread(buf, 512, 1, cfgFile)) {
            cfgData.insert(cfgData.end(), buf, buf + 512);
        }
    }
}

CFGParser::~CFGParser() {
    if (cfgFile) {
        fclose(cfgFile);
    }
}

void CFGParser::Parse() {
    if (!cfgFile || !cfgData.size())
        return;

    int state = ParserStateName;

    std::string headingName;
    std::string name;
    std::string value;

    std::vector<CFGItem> values;
    for (auto it = cfgData.begin(); it != cfgData.end(); it++) {
        char c = *it;
        switch (c) {
        case '=':
            if (state == ParserStateName) {
                state = ParserStateValue;
                break;
            } else {
                value += c;
                break;
            }
        case '#':
            while (it != cfgData.end() && (c = *it++) != '\n')
                ;
            [[fallthrough]];
        case '\n':
            if (state == ParserStateValue) {
                CFGItem item;

                item.name = name;
                item.value = value;

                // Trim whitespaces, tabs and carriage returns
                item.name.erase(0, name.find_first_not_of(" \t\r"));
                item.name.erase(name.find_last_not_of(" \t\r") + 1);
                item.value.erase(0, value.find_first_not_of(" \t\r"));
                item.value.erase(value.find_last_not_of(" \t\r") + 1);

                values.push_back(item);
            } else if (state == ParserStateName) {
                if (!name.length()) {
                    break; // Empty line?
                }

                CFGItem item;
                item.name = "";
                item.value = name;
            } else if (state == ParserStateHeading) {
                printf("CFGParser: Potentially malformed heading [%s\n", headingName.c_str());
                headingName.clear();
            }

            name.clear();
            value.clear();

            state = ParserStateName;
            break;
        case ']':
        case '[':
            if (c == '[' && state == ParserStateName && !name.length()) {
                state = ParserStateHeading;
                if (values.size() > 0) { // Don't add empty headings
                    items.push_back(std::pair<std::string, std::vector<CFGItem>>(headingName, values));
                }
                headingName.clear();
                values.clear();
            } else if (c == ']' && state == ParserStateHeading) {
                state = ParserStateName;
            }
            break;
        default:
            if (state == ParserStateHeading) {
                headingName += c;
            } else if (state == ParserStateName) {
                name += c;
            } else if (state == ParserStateValue) {
                value += c;
            }
            break;
        }
    }

    if (state == ParserStateValue) {
        CFGItem item;

        item.name = name;
        item.value = value;

        values.push_back(item);
    }

    items.push_back(std::pair<std::string, std::vector<CFGItem>>(headingName, values));
}
