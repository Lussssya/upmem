#define _POSIX_C_SOURCE 200809L

#include "workload_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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
    }
    if (sum > UINT32_MAX) {
        snprintf(error, error_capacity, "test sum must fit in uint32");
        return -1;
    }
    *result = (uint32_t)sum;
    return 0;
}

static int wait_for_dpu_runner(char *error, size_t error_capacity)
{
    const char *directory = getenv("OVERLAP_TEST_DIR");
    char cpu_marker[4096];
    char dpu_marker[4096];
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 10000000};
    FILE *marker;

    if (directory == NULL || directory[0] == '\0') {
        return 0;
    }
    if (snprintf(cpu_marker, sizeof(cpu_marker), "%s/cpu_started", directory) < 0 ||
        snprintf(dpu_marker, sizeof(dpu_marker), "%s/dpu_started", directory) < 0) {
        snprintf(error, error_capacity, "cannot build overlap marker path");
        return -1;
    }
    marker = fopen(cpu_marker, "w");
    if (marker == NULL) {
        snprintf(error, error_capacity, "cannot create CPU overlap marker");
        return -1;
    }
    fclose(marker);

    for (int attempt = 0; attempt < 500; attempt++) {
        if (access(dpu_marker, F_OK) == 0) {
            return 0;
        }
        nanosleep(&delay, NULL);
    }
    snprintf(error, error_capacity, "DPU runner did not overlap CPU worker");
    return -1;
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

static int compute_partition(
    const uint32_t *values,
    size_t count,
    uint32_t *result,
    char *error,
    size_t error_capacity)
{
    if (count == 1 && values[0] == 1 &&
        wait_for_dpu_runner(error, error_capacity) != 0) {
        return -1;
    }
    if (count == 1 && values[0] == 1 &&
        getenv("OVERLAP_CPU_FAIL") != NULL) {
        snprintf(error, error_capacity, "injected CPU backend failure");
        return -1;
    }
    return sum_values(values, count, result, error, error_capacity);
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
        snprintf(error, error_capacity, "test combined sum must fit in uint32");
        return -1;
    }
    *result = (uint32_t)combined;
    return 0;
}

static const WorkloadPluginV1 plugin = {
    .abi_version = WORKLOAD_PLUGIN_ABI_VERSION,
    .struct_size = sizeof(WorkloadPluginV1),
    .workload_id = "overlap",
    .name = "overlap_test",
    .description = "Test-only workload that verifies CPU and DPU overlap",
    .max_elements = 256,
    .identity = 0,
    .dpu_runner = "overlap_runner.sh",
    .dpu_kernel_wram = "../workloads/sum/dpu/wram/task.c",
    .dpu_kernel_mram = "../workloads/sum/dpu/mram/task.c",
    .validate_input = validate_input,
    .compute_partition = compute_partition,
    .combine = combine,
    .reference = sum_values,
};

const WorkloadPluginV1 *upmem_workload_plugin_v1(void)
{
    return &plugin;
}
