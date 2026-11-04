#include "workload_plugin.h"

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

static int maximum(
    const uint32_t *values,
    size_t count,
    uint32_t *result,
    char *error,
    size_t error_capacity)
{
    uint32_t maximum_value = 0;

    (void)error;
    (void)error_capacity;
    for (size_t i = 0; i < count; i++) {
        if (values[i] > maximum_value) {
            maximum_value = values[i];
        }
    }
    *result = maximum_value;
    return 0;
}

static int combine(
    uint32_t left,
    uint32_t right,
    uint32_t *result,
    char *error,
    size_t error_capacity)
{
    (void)error;
    (void)error_capacity;
    *result = left > right ? left : right;
    return 0;
}

static const WorkloadPluginV1 plugin = {
    .abi_version = WORKLOAD_PLUGIN_ABI_VERSION,
    .struct_size = sizeof(WorkloadPluginV1),
    .workload_id = "max",
    .name = "array_maximum",
    .description = "Maximum value in an unsigned 32-bit array",
    .max_elements = 256,
    .identity = 0,
    .dpu_runner = "run_scalar_workload.sh",
    .dpu_kernel_wram = "dpu/wram/task.c",
    .dpu_kernel_mram = "dpu/mram/task.c",
    .validate_input = validate_input,
    .compute_partition = maximum,
    .combine = combine,
    .reference = maximum,
};

const WorkloadPluginV1 *upmem_workload_plugin_v1(void)
{
    return &plugin;
}
