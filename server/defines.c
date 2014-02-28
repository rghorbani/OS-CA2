#include "defines.h"
void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}