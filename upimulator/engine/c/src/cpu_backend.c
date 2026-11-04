#include "cpu_backend.h"

#include <stdio.h>

int cpu_backend_execute(
    const WorkloadPluginV1 *workload,
    const uint32_t *values,
    size_t count,
    BackendResult *result,
    char *error,
    size_t error_capacity)
{
    result->partial_result = workload->identity;
    result->ran = count > 0;
    result->simulated_cycles = 0;

    if (count == 0) {
        return 0;
    }
    int status = workload->compute_partition(
        values,
        count,
        &result->partial_result,
        error,
        error_capacity);
    if (status != 0 && error_capacity > 0 && error[0] == '\0') {
        snprintf(error, error_capacity, "workload CPU computation failed");
    }
    return status;
}
