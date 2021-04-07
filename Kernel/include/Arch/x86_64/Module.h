#pragma once

#define MODULE_INFO_SYMBOL _moduleInfo

struct LemonModuleInfo {
    const char* name;
    const char* description;

    int (*init)(void);
    int (*exit)(void);
};

#define DECLARE_MODULE(mname, mdescription, minit, mexit) extern "C" { static LemonModuleInfo MODULE_INFO_SYMBOL __attribute__((used)) = { \
    .name = (mname), \
    .description = (mdescription), \
    .init = (minit), \
    .exit = (mexit) \
}; }