#pragma once

enum{
    DebugLevelQuiet = 0,
    DebugLevelNormal = 1,
    DebugLevelVerbose = 2,
};

extern const int debugLevelHAL;
extern const int debugLevelMisc;

extern const int debugLevelACPI;

extern const int debugLevelAHCI;
extern const int debugLevelATA;
extern const int debugLevelExt2;
extern const int debugLevelPartitions;

extern const int debugLevelXHCI;
extern const int debugLevelInterrupts;

#define KOBJECT_ASSERTIONS
#define REFPTR_DEBUG

#define REFPTR_ASSERTIONS
#define REFPTR_DEBUG