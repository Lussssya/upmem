#include "workload_plugin.h"

#include <stdio.h>

static int validate_input(
    const uint32_t *values,
    size_t count,
    char *error,
    size_t error_capacity)
{
    (void)values;
    (void)count;
    (void)error;
    (void)error_capacity;
    return 0;
}

static int count_even_values(
    const uint32_t *values,
    size_t count,
    uint32_t *result,
    char *error,
    size_t error_capacity)
{
    uint32_t even_count = 0;

    (void)error;
    (void)error_capacity;
    for (size_t i = 0; i < count; i++) {
        if ((values[i] & 1u) == 0u) {
            even_count++;
        }
    }
    *result = even_count;
    return 0;
}

static int combine(
    uint32_t left,
    uint32_t right,
    uint32_t *result,
    char *error,
    size_t error_capacity)
{
    uint64_t combined = (uint64_t)left + right;

    if (combined > UINT32_MAX) {
        snprintf(error, error_capacity, "combined even count must fit in uint32");
        return -1;
    }
    *result = (uint32_t)combined;
    return 0;
}

static const WorkloadPluginV1 plugin = {
    .abi_version = WORKLOAD_PLUGIN_ABI_VERSION,
    .struct_size = sizeof(WorkloadPluginV1),
    .workload_id = "count_even",
    .name = "count_even",
    .description = "Count even values in an unsigned 32-bit array",
    .max_elements = 256,
    .identity = 0,
    .dpu_runner = "run_scalar_workload.sh",
    .dpu_kernel_wram = "dpu/wram/task.c",
    .dpu_kernel_mram = "dpu/mram/task.c",
    .validate_input = validate_input,
    .compute_partition = count_even_values,
    .combine = combine,
    .reference = count_even_values,
};

const WorkloadPluginV1 *upmem_workload_plugin_v1(void)
{
    return &plugin;
}
