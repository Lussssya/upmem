#include "engine.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_backend.h"
#include "dpu_backend.h"
#include "input_loader.h"
#include "partitioner.h"
#include "workload_loader.h"

typedef struct {
    const WorkloadPluginV1 *workload;
    const uint32_t *values;
    size_t count;
    BackendResult result;
    char error[ENGINE_PATH_CAPACITY];
    int status;
} CpuWorkerContext;

typedef struct {
    LoadedWorkload workload;
    int workload_loaded;
    pthread_t cpu_thread;
    int cpu_thread_started;
    CpuWorkerContext cpu_worker;
} EngineResources;

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

static void copy_error(
    char *destination,
    size_t destination_capacity,
    const char *source,
    const char *fallback)
{
    snprintf(
        destination,
        destination_capacity,
        "%s",
        source[0] == '\0' ? fallback : source);
}

static void *cpu_worker_main(void *argument)
{
    CpuWorkerContext *worker = argument;

    worker->error[0] = '\0';
    worker->status = cpu_backend_execute(
        worker->workload,
        worker->values,
        worker->count,
        &worker->result,
        worker->error,
        sizeof(worker->error));
    return NULL;
}

static void wait_for_cpu_worker(EngineResources *resources)
{
    int join_status;

    if (!resources->cpu_thread_started) {
        return;
    }
    join_status = pthread_join(resources->cpu_thread, NULL);
    if (join_status != 0) {
        /*
         * Returning is unsafe: the worker may still reference this stack and
         * execute plugin code. Terminate without stack unwinding or dlclose().
         */
        fprintf(
            stderr,
            "FATAL: cannot wait for CPU worker: %s; terminating without unloading plugin\n",
            strerror(join_status));
        fflush(stderr);
        _Exit(EXIT_FAILURE);
    }
    resources->cpu_thread_started = 0;
}

static int finish_execution(
    EngineResources *resources,
    int status)
{
    wait_for_cpu_worker(resources);
    if (resources->workload_loaded) {
        workload_unload(&resources->workload);
        resources->workload_loaded = 0;
    }
    return status;
}

static int compute_dpu_expected_result(
    const LoadedWorkload *workload,
    const WorkloadInput *input,
    const WorkPartition *partition,
    uint32_t *expected_result,
    char *error,
    size_t error_capacity)
{
    *expected_result = workload->api->identity;
    if (partition->dpu_count == 0) {
        return 0;
    }
    if (workload->api->compute_partition(
            input->values + partition->dpu_offset,
            partition->dpu_count,
            expected_result,
            error,
            error_capacity) != 0) {
        set_callback_error(
            error,
            error_capacity,
            "workload DPU validation computation failed");
        return -1;
    }
    return 0;
}

static int execute_sequential_backends(
    const EngineConfig *config,
    const LoadedWorkload *workload,
    const WorkloadInput *input,
    const WorkPartition *partition,
    BackendResult *cpu_result,
    BackendResult *dpu_result,
    char *error,
    size_t error_capacity)
{
    uint32_t dpu_expected_result;

    if (cpu_backend_execute(
            workload->api,
            input->values + partition->cpu_offset,
            partition->cpu_count,
            cpu_result,
            error,
            error_capacity) != 0) {
        return -1;
    }
    if (compute_dpu_expected_result(
            workload,
            input,
            partition,
            &dpu_expected_result,
            error,
            error_capacity) != 0) {
        return -1;
    }
    return dpu_backend_execute(
        config->upimulator_root,
        workload,
        config->dpu_memory,
        config->tasklets,
        input->values + partition->dpu_offset,
        partition->dpu_count,
        dpu_expected_result,
        dpu_result,
        error,
        error_capacity);
}

static int execute_hybrid_backends(
    const EngineConfig *config,
    EngineResources *resources,
    const WorkloadInput *input,
    const WorkPartition *partition,
    BackendResult *cpu_result,
    BackendResult *dpu_result,
    char *error,
    size_t error_capacity)
{
    CpuWorkerContext *worker = &resources->cpu_worker;
    uint32_t dpu_expected_result;
    char dpu_error[ENGINE_PATH_CAPACITY] = "";
    int create_status;
    int dpu_status;

    if (compute_dpu_expected_result(
            &resources->workload,
            input,
            partition,
            &dpu_expected_result,
            error,
            error_capacity) != 0) {
        return -1;
    }

    worker->workload = resources->workload.api;
    worker->values = input->values + partition->cpu_offset;
    worker->count = partition->cpu_count;
    worker->error[0] = '\0';
    worker->status = -1;
    create_status = pthread_create(
        &resources->cpu_thread,
        NULL,
        cpu_worker_main,
        worker);
    if (create_status != 0) {
        snprintf(
            error,
            error_capacity,
            "cannot start CPU worker: %s",
            strerror(create_status));
        return -1;
    }
    resources->cpu_thread_started = 1;

    dpu_status = dpu_backend_execute(
        config->upimulator_root,
        &resources->workload,
        config->dpu_memory,
        config->tasklets,
        input->values + partition->dpu_offset,
        partition->dpu_count,
        dpu_expected_result,
        dpu_result,
        dpu_error,
        sizeof(dpu_error));

    wait_for_cpu_worker(resources);
    *cpu_result = worker->result;
    if (worker->status != 0) {
        const char *cpu_error = worker->error[0] == '\0'
            ? "workload CPU computation failed"
            : worker->error;

        if (dpu_status != 0) {
            const char *dpu_failure = dpu_error[0] == '\0'
                ? "uPIMulator execution failed"
                : dpu_error;
            snprintf(
                error,
                error_capacity,
                "%s; DPU backend also failed: %s",
                cpu_error,
                dpu_failure);
        } else {
            copy_error(
                error,
                error_capacity,
                cpu_error,
                "workload CPU computation failed");
        }
        return -1;
    }
    if (dpu_status != 0) {
        copy_error(
            error,
            error_capacity,
            dpu_error,
            "uPIMulator execution failed");
        return -1;
    }
    return 0;
}

int engine_execute(
    const EngineConfig *config,
    ExecutionResult *result,
    char *error,
    size_t error_capacity)
{
    EngineResources resources;
    WorkloadInput input;
    WorkPartition partition;
    BackendResult cpu_result;
    BackendResult dpu_result;

    if (config == NULL || result == NULL || error == NULL || error_capacity == 0) {
        return -1;
    }
    error[0] = '\0';
    memset(&resources, 0, sizeof(resources));

    if (workload_load(
            config->workload_path,
            &resources.workload,
            error,
            error_capacity) != 0) {
        return -1;
    }
    resources.workload_loaded = 1;

    if (input_load(config->input_path, &input, error, error_capacity) != 0) {
        return finish_execution(&resources, -1);
    }
    if (input.count > resources.workload.api->max_elements) {
        snprintf(
            error,
            error_capacity,
            "workload %s supports at most %u elements",
            resources.workload.api->name,
            resources.workload.api->max_elements);
        return finish_execution(&resources, -1);
    }
    if (resources.workload.api->validate_input(
            input.values,
            input.count,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload input validation failed");
        return finish_execution(&resources, -1);
    }
    if (partition_workload(
            input.count,
            config->mode,
            config->cpu_share,
            &partition,
            error,
            error_capacity) != 0) {
        return finish_execution(&resources, -1);
    }

    if (config->mode == MODE_HYBRID) {
        if (execute_hybrid_backends(
                config,
                &resources,
                &input,
                &partition,
                &cpu_result,
                &dpu_result,
                error,
                error_capacity) != 0) {
            return finish_execution(&resources, -1);
        }
    } else if (execute_sequential_backends(
                   config,
                   &resources.workload,
                   &input,
                   &partition,
                   &cpu_result,
                   &dpu_result,
                   error,
                   error_capacity) != 0) {
        return finish_execution(&resources, -1);
    }

    if (resources.workload.api->combine(
            cpu_result.partial_result,
            dpu_result.partial_result,
            &result->combined_result,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload result combination failed");
        return finish_execution(&resources, -1);
    }
    if (resources.workload.api->reference(
            input.values,
            input.count,
            &result->reference_result,
            error,
            error_capacity) != 0) {
        set_callback_error(error, error_capacity, "workload reference computation failed");
        return finish_execution(&resources, -1);
    }

    if (snprintf(
            result->workload_name,
            sizeof(result->workload_name),
            "%s",
            resources.workload.api->name) >= (int)sizeof(result->workload_name)) {
        snprintf(error, error_capacity, "workload display name is too long");
        return finish_execution(&resources, -1);
    }
    result->total_elements = input.count;
    result->cpu_elements = partition.cpu_count;
    result->dpu_elements = partition.dpu_count;
    result->cpu_partial_result = cpu_result.partial_result;
    result->dpu_partial_result = dpu_result.partial_result;
    result->correct = result->combined_result == result->reference_result;
    result->dpu_ran = dpu_result.ran;
    result->dpu_simulated_cycles = dpu_result.simulated_cycles;
    return finish_execution(&resources, 0);
}
