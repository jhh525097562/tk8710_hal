#include "web_server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *PROGRAM_VERSION = "V1.0.1";

static void signal_handler(int sig) {
    exit(0);
}

static void print_program_version(void) {
    printf("%s\n", PROGRAM_VERSION);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "-ver") == 0 || strcmp(argv[1], "--version") == 0)) {
        print_program_version();
        return 0;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    web_server_start("http://0.0.0.0:8090");
    return 0;
}
