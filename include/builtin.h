#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

typedef enum {
    BUILTIN_NOT_FOUND = 0, /* argv[0] is not a builtin - caller should fork/exec */
    BUILTIN_HANDLED,       /* builtin ran, shell should keep looping normally   */
    BUILTIN_EXIT           /* the "exit" builtin was run - shell should stop    */
} builtin_status_t;

int is_builtin(const char *name);
builtin_status_t execute_builtin(command_t *cmd);

#endif
