#ifndef WORKLOAD_PLUGIN_H
#define WORKLOAD_PLUGIN_H

#include <stddef.h>
#include <stdint.h>

#define WORKLOAD_PLUGIN_ABI_VERSION 1u
#define WORKLOAD_PLUGIN_ENTRYPOINT "upmem_workload_plugin_v1"
#define WORKLOAD_ID_MAX_LENGTH 63u

/*
 * Public ABI for uint32_t array -> uint32_t scalar workloads.
 *
 * Every callback returns 0 on success and -1 on failure. On failure it should
 * place a human-readable message in error when error_capacity is nonzero.
 * compute_partition() and reference() must implement the same mathematical
 * operation. combine() combines two partition results. The identity is used
 * for empty CPU or DPU partitions.
 *
 * Kernel paths are relative to the directory containing the plugin shared
 * library unless absolute. The runner path is relative to the installed
 * uPIMulator root unless absolute.
 */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;

    const char *workload_id;
    const char *name;
    const char *description;
    uint32_t max_elements;
    uint32_t identity;

    const char *dpu_runner;
    const char *dpu_kernel_wram;
    const char *dpu_kernel_mram;

    int (*validate_input)(
        const uint32_t *values,
        size_t count,
        char *error,
        size_t error_capacity);
    int (*compute_partition)(
        const uint32_t *values,
        size_t count,
        uint32_t *result,
        char *error,
        size_t error_capacity);
    int (*combine)(
        uint32_t left,
        uint32_t right,
        uint32_t *result,
        char *error,
        size_t error_capacity);
    int (*reference)(
        const uint32_t *values,
        size_t count,
        uint32_t *result,
        char *error,
        size_t error_capacity);
} WorkloadPluginV1;

typedef const WorkloadPluginV1 *(*WorkloadPluginEntrypointV1)(void);

#endif
