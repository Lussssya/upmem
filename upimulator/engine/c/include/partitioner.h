#ifndef PARTITIONER_H
#define PARTITIONER_H

#include <stddef.h>

#include "engine_types.h"

int partition_workload(
    size_t element_count,
    ExecutionMode mode,
    long double cpu_share,
    WorkPartition *partition,
    char *error,
    size_t error_capacity);

#endif
