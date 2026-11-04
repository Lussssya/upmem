#ifndef ENGINE_H
#define ENGINE_H

#include <stddef.h>

#include "engine_types.h"

int engine_execute(
    const EngineConfig *config,
    ExecutionResult *result,
    char *error,
    size_t error_capacity);

const char *execution_mode_name(ExecutionMode mode);

#endif
