#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

/* Set to 1 by execute_command() when the "exit" builtin runs. main.c's
   REPL loop checks this flag after each execute_pipeline()/execute_command()
   call and breaks out if it's set - this is separate from the int return
   value, which (per MILESTONE-4.1) now carries an actual exit status
   rather than a "should the shell stop" boolean. */
extern int shell_exit_requested;

int execute_command(command_t *cmd);
int execute_pipeline(pipeline_t *pipeline);

#endif
