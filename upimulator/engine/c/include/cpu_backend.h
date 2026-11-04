#ifndef CPU_BACKEND_H
#define CPU_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "engine_types.h"
#include "workload_plugin.h"

int cpu_backend_execute(
    const WorkloadPluginV1 *workload,
    const uint32_t *values,
    size_t count,
    BackendResult *result,
    char *error,
    size_t error_capacity);

#endif
