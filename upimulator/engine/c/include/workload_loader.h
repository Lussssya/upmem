#ifndef WORKLOAD_LOADER_H
#define WORKLOAD_LOADER_H

#include <stddef.h>

#include "engine_types.h"
#include "workload_plugin.h"

typedef struct {
    void *shared_library;
    const WorkloadPluginV1 *api;
    char plugin_directory[ENGINE_PATH_CAPACITY];
} LoadedWorkload;

int workload_load(
    const char *path,
    LoadedWorkload *workload,
    char *error,
    size_t error_capacity);
void workload_unload(LoadedWorkload *workload);
int workload_resolve_kernel(
    const LoadedWorkload *workload,
    DpuMemory memory,
    char *path,
    size_t path_capacity,
    char *error,
    size_t error_capacity);

#endif
