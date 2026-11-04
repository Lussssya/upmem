#ifndef DPU_BACKEND_H
#define DPU_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "engine_types.h"
#include "workload_loader.h"

int dpu_backend_execute(
    const char *upimulator_root,
    const LoadedWorkload *workload,
    DpuMemory memory,
    unsigned int tasklets,
    const uint32_t *values,
    size_t count,
    uint32_t expected_result,
    BackendResult *result,
    char *error,
    size_t error_capacity);

const char *dpu_memory_name(DpuMemory memory);

#endif
