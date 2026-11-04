#include "partitioner.h"

#include <stdio.h>

int partition_workload(
    size_t element_count,
    ExecutionMode mode,
    long double cpu_share,
    WorkPartition *partition,
    char *error,
    size_t error_capacity)
{
    size_t cpu_count;

    switch (mode) {
    case MODE_CPU:
        cpu_count = element_count;
        break;
    case MODE_DPU:
        cpu_count = 0;
        break;
    case MODE_HYBRID:
        if (cpu_share < 0.0L || cpu_share > 1.0L) {
            snprintf(error, error_capacity, "CPU share must be between 0 and 1");
            return -1;
        }
        if (cpu_share == 1.0L) {
            cpu_count = element_count;
        } else {
            cpu_count = (size_t)((long double)element_count * cpu_share);
        }
        break;
    default:
        snprintf(error, error_capacity, "unsupported execution mode");
        return -1;
    }

    partition->cpu_offset = 0;
    partition->cpu_count = cpu_count;
    partition->dpu_offset = cpu_count;
    partition->dpu_count = element_count - cpu_count;
    return 0;
}
