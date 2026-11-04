#include "engine.h"

#include <stdio.h>
#include <string.h>

#include "cpu_backend.h"
#include "dpu_backend.h"
#include "input_loader.h"
#include "partitioner.h"
#include "workload_loader.h"

const char *execution_mode_name(ExecutionMode mode)
{
    switch (mode) {
    case MODE_CPU:
        return "CPU";
    case MODE_DPU:
        return "DPU";
    case MODE_HYBRID:
        return "HYBRID";
    default:
        return "UNKNOWN";
    }
}

static void set_callback_error(
    char *error,
    size_t error_capacity,
    const char *fallback)
{
    if (error_capacity > 0 && error[0] == '\0') {
        snprintf(error, error_capacity, "%s", fallback);
    }
}

int engine_execute(
    const EngineConfig *config,
    ExecutionResult *result,
    char *error,
    size_t error_capacity)
{
    LoadedWorkload workload;
    WorkloadInput input;
    WorkPartition partition;
    BackendResult cpu_result;
    BackendResult dpu_result;
    uint32_t dpu_expected_result;
    int status = -1;

    if (config == NULL || result == NULL || error == NULL || error_capacity == 0) {
        return -1;
    }
    error[0] = '\0';

    if (workload_load(
            config->workload_path,
            &workload,
            error,
            error_capacity) != 0) {
        return -1;
    }

    if (input_load(config->input_path, &input, error, error_capacity) != 0) {
        goto cleanup;
    }
    if (input.count > workload.api->max_elements) {
        snprintf(
            error,
            error_capacity,
            "workload %s supports at most %u elements",
            workload.api->name,
            workload.api->max_elements);
        goto cleanup;
    }
    if (workload.api->validate_input(
            input.values,
            input.count,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload input validation failed");
        goto cleanup;
    }
    if (partition_workload(
            input.count,
            config->mode,
            config->cpu_share,
            &partition,
            error,
            error_capacity) != 0) {
        goto cleanup;
    }

    if (cpu_backend_execute(
            workload.api,
            input.values + partition.cpu_offset,
            partition.cpu_count,
            &cpu_result,
            error,
            error_capacity) != 0) {
        goto cleanup;
    }
    dpu_expected_result = workload.api->identity;
    if (partition.dpu_count > 0 &&
        workload.api->compute_partition(
            input.values + partition.dpu_offset,
            partition.dpu_count,
            &dpu_expected_result,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload DPU validation computation failed");
        goto cleanup;
    }
    if (dpu_backend_execute(
            config->upimulator_root,
            &workload,
            config->dpu_memory,
            config->tasklets,
            input.values + partition.dpu_offset,
            partition.dpu_count,
            dpu_expected_result,
            &dpu_result,
            error,
            error_capacity) != 0) {
        goto cleanup;
    }
    if (workload.api->combine(
            cpu_result.partial_result,
            dpu_result.partial_result,
            &result->combined_result,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload result combination failed");
        goto cleanup;
    }
    if (workload.api->reference(
            input.values,
            input.count,
            &result->reference_result,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload reference computation failed");
        goto cleanup;
    }

    if (snprintf(
            result->workload_name,
            sizeof(result->workload_name),
            "%s",
            workload.api->name) >= (int)sizeof(result->workload_name)) {
        snprintf(error, error_capacity, "workload display name is too long");
        goto cleanup;
    }
    result->total_elements = input.count;
    result->cpu_elements = partition.cpu_count;
    result->dpu_elements = partition.dpu_count;
    result->cpu_partial_result = cpu_result.partial_result;
    result->dpu_partial_result = dpu_result.partial_result;
    result->correct = result->combined_result == result->reference_result;
    result->dpu_ran = dpu_result.ran;
    result->dpu_simulated_cycles = dpu_result.simulated_cycles;
    status = 0;

cleanup:
    workload_unload(&workload);
    return status;
}
