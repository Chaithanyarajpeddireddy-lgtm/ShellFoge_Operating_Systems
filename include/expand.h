#ifndef EXPAND_H
#define EXPAND_H

#include "parser.h"

/* Expands $VAR, $?, and ~ in every argument, input file, and output file
   across all commands in the pipeline. */
void expand_variables(pipeline_t *pipeline);

#endif
