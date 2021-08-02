#pragma once

#include <Compiler.h>

typedef void (*__sighandler) (int);
#include <abi-bits/signal.h>

#define SIGNAL_MAX 34 // Maximum amount of signal handlers

struct SignalHandler {
    enum {
        ActionDefault = 0, // Default signal action
        ActionIgnore = 1, // Ignore signal entirely
        ActionUsermodeHandler = 2, // Execute usermode handler
    };

    enum {
        FlagSignalInfo = SA_SIGINFO, // Use sa_sigaction instead of sa_handler
        FlagNoChildStop = SA_NOCLDSTOP, // If SIGCHLD do not receive notification when child processes stop
        FlagNoChildWait = SA_NOCLDWAIT, // If SIGCHLD do not transform children into zombies when they terminate
        FlagNoDefer = SA_NODEFER, // Do not add to the signal mask whilst the handler is executing
        FlagOnStack = SA_ONSTACK, // Call on an alternate signal stack
        FlagResetHand = SA_RESETHAND, // Restore signal action to default afterwards
        FlagRestart = SA_RESTART, // Make system calls restartable across signals
    };
    static const int supportedFlags = 0; // Which flags do we support
    
    int action = ActionDefault;
    int flags = 0;

    // The mask that is set when the signal is raised
    // The old mask should be restored by sigreturn
    sigset_t mask = 0;

    void* userHandler = nullptr;

    ALWAYS_INLINE bool Ignore() const { return action == ActionIgnore; }   
};