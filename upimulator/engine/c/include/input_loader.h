#ifndef INPUT_LOADER_H
#define INPUT_LOADER_H

#include <stddef.h>

#include "engine_types.h"

int input_load(
    const char *path,
    WorkloadInput *input,
    char *error,
    size_t error_capacity);

#endif
