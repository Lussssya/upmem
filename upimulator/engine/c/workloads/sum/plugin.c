#include "workload_plugin.h"

#include <stdio.h>

static int sum_values(
    const uint32_t *values,
    size_t count,
    uint32_t *result,
    char *error,
    size_t error_capacity)
{
    uint64_t sum = 0;

    for (size_t i = 0; i < count; i++) {
        sum += values[i];
        if (sum > UINT32_MAX) {
            snprintf(error, error_capacity, "array sum must fit in uint32");
            return -1;
        }
    }
    *result = (uint32_t)sum;
    return 0;
}

static int validate_input(
    const uint32_t *values,
    size_t count,
    char *error,
    size_t error_capacity)
{
    uint32_t ignored;
    return sum_values(values, count, &ignored, error, error_capacity);
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
        snprintf(error, error_capacity, "combined sum must fit in uint32");
        return -1;
    }
    *result = (uint32_t)combined;
    return 0;
}

static const WorkloadPluginV1 plugin = {
    .abi_version = WORKLOAD_PLUGIN_ABI_VERSION,
    .struct_size = sizeof(WorkloadPluginV1),
    .workload_id = "sum",
    .name = "array_sum",
    .description = "Unsigned 32-bit array sum with checked uint32 result",
    .max_elements = 256,
    .identity = 0,
    .dpu_runner = "run_scalar_workload.sh",
    .dpu_kernel_wram = "dpu/wram/task.c",
    .dpu_kernel_mram = "dpu/mram/task.c",
    .validate_input = validate_input,
    .compute_partition = sum_values,
    .combine = combine,
    .reference = sum_values,
};

const WorkloadPluginV1 *upmem_workload_plugin_v1(void)
{
    return &plugin;
}
