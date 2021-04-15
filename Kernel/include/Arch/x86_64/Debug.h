#pragma once

enum{
    DebugLevelQuiet = 0,
    DebugLevelNormal = 1,
    DebugLevelVerbose = 2,
};

#define KERNEL_DEBUG

#ifdef KERNEL_DEBUG
    #define IF_DEBUG(a, b) if(a) b
#else
    #define IF_DEBUG(a, b)
#endif

extern const int debugLevelHAL;
extern const int debugLevelScheduler;
extern const int debugLevelMisc;
extern const int debugLevelModules;
extern const int debugLevelSymbols;

extern const int debugLevelACPI;

extern const int debugLevelAHCI;
extern const int debugLevelATA;
extern const int debugLevelNVMe;
extern const int debugLevelPartitions;

extern const int debugLevelFilesystem;
extern const int debugLevelTmpFS;
extern const int debugLevelExt2;

extern const int debugLevelXHCI;
extern const int debugLevelInterrupts;

extern const int debugLevelMessageEndpoint;

extern const int debugLevelSyscalls;
extern const int debugLevelUsermodeMM;

extern const int debugLevelNetwork;

#define REFPTR_ASSERTIONS
//#define REFPTR_DEBUG