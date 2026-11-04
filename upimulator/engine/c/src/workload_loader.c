#define _POSIX_C_SOURCE 200809L

#include "workload_loader.h"

#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int copy_path(
    char *destination,
    size_t capacity,
    const char *source,
    char *error,
    size_t error_capacity)
{
    int length = snprintf(destination, capacity, "%s", source);
    if (length < 0 || (size_t)length >= capacity) {
        snprintf(error, error_capacity, "workload path is too long");
        return -1;
    }
    return 0;
}

static int valid_workload_id(const char *id)
{
    size_t length;

    if (id == NULL || id[0] == '\0') {
        return 0;
    }
    length = strlen(id);
    if (length > WORKLOAD_ID_MAX_LENGTH) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        unsigned char character = (unsigned char)id[i];
        if (!isalnum(character) && character != '_' && character != '-') {
            return 0;
        }
    }
    return 1;
}

static int validate_api(
    const WorkloadPluginV1 *api,
    char *error,
    size_t error_capacity)
{
    if (api == NULL) {
        snprintf(error, error_capacity, "workload plugin returned a null interface");
        return -1;
    }
    if (api->abi_version != WORKLOAD_PLUGIN_ABI_VERSION) {
        snprintf(
            error,
            error_capacity,
            "incompatible workload plugin ABI: expected %u, found %u",
            WORKLOAD_PLUGIN_ABI_VERSION,
            api->abi_version);
        return -1;
    }
    if (api->struct_size < sizeof(WorkloadPluginV1)) {
        snprintf(error, error_capacity, "workload plugin interface is too small");
        return -1;
    }
    if (!valid_workload_id(api->workload_id)) {
        snprintf(error, error_capacity, "workload plugin has an invalid workload_id");
        return -1;
    }
    if (api->name == NULL || api->name[0] == '\0' ||
        api->description == NULL || api->description[0] == '\0' ||
        api->dpu_runner == NULL || api->dpu_runner[0] == '\0' ||
        api->dpu_kernel_wram == NULL || api->dpu_kernel_wram[0] == '\0' ||
        api->dpu_kernel_mram == NULL || api->dpu_kernel_mram[0] == '\0' ||
        api->validate_input == NULL || api->compute_partition == NULL ||
        api->combine == NULL || api->reference == NULL) {
        snprintf(error, error_capacity, "workload plugin is missing required metadata or callbacks");
        return -1;
    }
    if (strlen(api->name) >= ENGINE_WORKLOAD_NAME_CAPACITY) {
        snprintf(error, error_capacity, "workload display name is too long");
        return -1;
    }
    if (api->max_elements == 0 || api->max_elements > ENGINE_MAX_ELEMENTS) {
        snprintf(
            error,
            error_capacity,
            "workload max_elements must be from 1 through %d",
            ENGINE_MAX_ELEMENTS);
        return -1;
    }
    return 0;
}

int workload_load(
    const char *path,
    LoadedWorkload *workload,
    char *error,
    size_t error_capacity)
{
    char resolved[ENGINE_PATH_CAPACITY];
    char *last_separator;
    void *symbol;
    WorkloadPluginEntrypointV1 entrypoint = NULL;
    const char *loader_error;

    memset(workload, 0, sizeof(*workload));
    if (path == NULL || path[0] == '\0') {
        snprintf(error, error_capacity, "a workload plugin path is required");
        return -1;
    }
    if (realpath(path, resolved) == NULL) {
        snprintf(error, error_capacity, "cannot resolve workload plugin %s", path);
        return -1;
    }

    workload->shared_library = dlopen(resolved, RTLD_NOW | RTLD_LOCAL);
    if (workload->shared_library == NULL) {
        snprintf(error, error_capacity, "cannot load workload plugin %s: %s", resolved, dlerror());
        return -1;
    }

    dlerror();
    symbol = dlsym(workload->shared_library, WORKLOAD_PLUGIN_ENTRYPOINT);
    loader_error = dlerror();
    if (loader_error != NULL || symbol == NULL) {
        snprintf(
            error,
            error_capacity,
            "workload plugin is missing %s: %s",
            WORKLOAD_PLUGIN_ENTRYPOINT,
            loader_error == NULL ? "symbol not found" : loader_error);
        workload_unload(workload);
        return -1;
    }
    if (sizeof(entrypoint) != sizeof(symbol)) {
        snprintf(error, error_capacity, "workload plugin function pointer size is incompatible");
        workload_unload(workload);
        return -1;
    }
    memcpy(&entrypoint, &symbol, sizeof(entrypoint));
    workload->api = entrypoint();
    if (validate_api(workload->api, error, error_capacity) != 0) {
        workload_unload(workload);
        return -1;
    }

    last_separator = strrchr(resolved, '/');
    if (last_separator == NULL) {
        snprintf(error, error_capacity, "cannot determine workload plugin directory");
        workload_unload(workload);
        return -1;
    }
    *last_separator = '\0';
    if (copy_path(
            workload->plugin_directory,
            sizeof(workload->plugin_directory),
            resolved,
            error,
            error_capacity) != 0) {
        workload_unload(workload);
        return -1;
    }
    return 0;
}

void workload_unload(LoadedWorkload *workload)
{
    if (workload->shared_library != NULL) {
        dlclose(workload->shared_library);
    }
    workload->shared_library = NULL;
    workload->api = NULL;
    workload->plugin_directory[0] = '\0';
}

int workload_resolve_kernel(
    const LoadedWorkload *workload,
    DpuMemory memory,
    char *path,
    size_t path_capacity,
    char *error,
    size_t error_capacity)
{
    const char *configured;
    int length;

    if (memory == DPU_MEMORY_WRAM) {
        configured = workload->api->dpu_kernel_wram;
    } else if (memory == DPU_MEMORY_MRAM) {
        configured = workload->api->dpu_kernel_mram;
    } else {
        snprintf(error, error_capacity, "invalid DPU memory mode");
        return -1;
    }

    if (configured[0] == '/') {
        length = snprintf(path, path_capacity, "%s", configured);
    } else {
        length = snprintf(
            path,
            path_capacity,
            "%s/%s",
            workload->plugin_directory,
            configured);
    }
    if (length < 0 || (size_t)length >= path_capacity) {
        snprintf(error, error_capacity, "DPU kernel path is too long");
        return -1;
    }
    if (access(path, R_OK) != 0) {
        snprintf(error, error_capacity, "DPU kernel is not readable: %s", path);
        return -1;
    }
    return 0;
}
