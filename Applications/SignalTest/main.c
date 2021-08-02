#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void sigchld_handler(int signal) {
    printf("sigchld_handler: Received signal %d!\n", signal);
}

int main() {
    pid_t child = fork();
    if (!child) {
        for (;;)
            ; // Wait for SIGINT
    } else {
        struct sigaction action;
        action.sa_sigaction = NULL;
        action.sa_handler = sigchld_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        sigaction(SIGCHLD, &action, NULL);

        kill(child, SIGINT);

        if (waitpid(-1, NULL, 0) < 0) {
            perror("waitpid");
        }
    }

    return 0;
}
