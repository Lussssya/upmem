#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpu_backend.h"
#include "engine.h"
#include "workload_loader.h"

static void print_usage(FILE *stream, const char *program)
{
    fprintf(
        stream,
        "Usage: %s --workload PLUGIN.so --mode cpu|dpu|hybrid --input FILE [options]\n"
        "\n"
        "Options:\n"
        "  --workload FILE        Runtime-loadable workload plugin\n"
        "  --cpu-share X          Required for hybrid mode; from 0 through 1\n"
        "  --dpu-memory MODE      wram or mram (default: wram)\n"
        "  --tasklets N           DPU tasklets from 1 through 24 (default: 1)\n"
        "  --upimulator-root DIR  Installed Go uPIMulator root\n"
        "  --describe-workload    Print plugin metadata and exit\n"
        "  --help                  Show this help\n",
        program);
}

static int describe_workload(const char *path, char *error, size_t error_capacity)
{
    LoadedWorkload workload;
    char kernel_path[ENGINE_PATH_CAPACITY];
    char kernel_error[512];
    int wram_available;
    int mram_available;

    if (workload_load(path, &workload, error, error_capacity) != 0) {
        return -1;
    }

    wram_available = workload_resolve_kernel(
        &workload,
        DPU_MEMORY_WRAM,
        kernel_path,
        sizeof(kernel_path),
        kernel_error,
        sizeof(kernel_error)) == 0;
    mram_available = workload_resolve_kernel(
        &workload,
        DPU_MEMORY_MRAM,
        kernel_path,
        sizeof(kernel_path),
        kernel_error,
        sizeof(kernel_error)) == 0;

    printf("Workload ID: %s\n", workload.api->workload_id);
    printf("Name: %s\n", workload.api->name);
    printf("Description: %s\n", workload.api->description);
    printf("Plugin ABI: %" PRIu32 "\n", workload.api->abi_version);
    printf("Input type: uint32_t array\n");
    printf("Supported elements: 1..%" PRIu32 "\n", workload.api->max_elements);
    printf("Empty-partition identity: %" PRIu32 "\n", workload.api->identity);
    printf("WRAM kernel available: %s\n", wram_available ? "yes" : "no");
    printf("MRAM kernel available: %s\n", mram_available ? "yes" : "no");

    workload_unload(&workload);
    return 0;
}

static int copy_text(char *destination, size_t capacity, const char *source)
{
    int length = snprintf(destination, capacity, "%s", source);
    return length >= 0 && (size_t)length < capacity ? 0 : -1;
}

static int parse_mode(const char *text, ExecutionMode *mode)
{
    if (strcmp(text, "cpu") == 0) {
        *mode = MODE_CPU;
    } else if (strcmp(text, "dpu") == 0) {
        *mode = MODE_DPU;
    } else if (strcmp(text, "hybrid") == 0) {
        *mode = MODE_HYBRID;
    } else {
        return -1;
    }
    return 0;
}

static int parse_memory(const char *text, DpuMemory *memory)
{
    if (strcmp(text, "wram") == 0) {
        *memory = DPU_MEMORY_WRAM;
    } else if (strcmp(text, "mram") == 0) {
        *memory = DPU_MEMORY_MRAM;
    } else {
        return -1;
    }
    return 0;
}

static int parse_share(const char *text, long double *share)
{
    char *end = NULL;

    errno = 0;
    *share = strtold(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(*share) ||
        *share < 0.0L || *share > 1.0L) {
        return -1;
    }
    return 0;
}

static int parse_tasklets(const char *text, unsigned int *tasklets)
{
    char *end = NULL;
    unsigned long value;

    if (text[0] == '-' || text[0] == '+') {
        return -1;
    }
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 1 || value > 24) {
        return -1;
    }
    *tasklets = (unsigned int)value;
    return 0;
}

static int set_default_root(EngineConfig *config)
{
    const char *configured = getenv("UPIMULATOR_ROOT");
    const char *home;
    int length;

    if (configured != NULL && configured[0] != '\0') {
        return copy_text(config->upimulator_root, sizeof(config->upimulator_root), configured);
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return -1;
    }
    length = snprintf(
        config->upimulator_root,
        sizeof(config->upimulator_root),
        "%s/pim/upimulator/golang/uPIMulator",
        home);
    return length >= 0 && (size_t)length < sizeof(config->upimulator_root) ? 0 : -1;
}

int main(int argc, char **argv)
{
    static const struct option options[] = {
        {"mode", required_argument, NULL, 'm'},
        {"workload", required_argument, NULL, 'w'},
        {"cpu-share", required_argument, NULL, 's'},
        {"dpu-memory", required_argument, NULL, 'd'},
        {"tasklets", required_argument, NULL, 't'},
        {"input", required_argument, NULL, 'i'},
        {"upimulator-root", required_argument, NULL, 'r'},
        {"describe-workload", no_argument, NULL, 'D'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    EngineConfig config = {
        .mode = MODE_CPU,
        .mode_set = false,
        .cpu_share = 0.0L,
        .cpu_share_set = false,
        .dpu_memory = DPU_MEMORY_WRAM,
        .tasklets = 1,
        .workload_path = "",
        .input_path = "",
        .upimulator_root = "",
    };
    ExecutionResult result;
    char error[4096];
    int option;
    int describe = 0;

    if (set_default_root(&config) != 0) {
        fprintf(stderr, "ERROR: cannot determine the default uPIMulator path\n");
        return 2;
    }

    while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
        switch (option) {
        case 'w':
            if (copy_text(config.workload_path, sizeof(config.workload_path), optarg) != 0) {
                fprintf(stderr, "ERROR: workload plugin path is too long\n");
                return 2;
            }
            break;
        case 'm':
            if (parse_mode(optarg, &config.mode) != 0) {
                fprintf(stderr, "ERROR: mode must be cpu, dpu, or hybrid\n");
                return 2;
            }
            config.mode_set = true;
            break;
        case 's':
            if (parse_share(optarg, &config.cpu_share) != 0) {
                fprintf(stderr, "ERROR: CPU share must be a number between 0 and 1\n");
                return 2;
            }
            config.cpu_share_set = true;
            break;
        case 'd':
            if (parse_memory(optarg, &config.dpu_memory) != 0) {
                fprintf(stderr, "ERROR: DPU memory must be wram or mram\n");
                return 2;
            }
            break;
        case 't':
            if (parse_tasklets(optarg, &config.tasklets) != 0) {
                fprintf(stderr, "ERROR: tasklets must be an integer from 1 through 24\n");
                return 2;
            }
            break;
        case 'i':
            if (copy_text(config.input_path, sizeof(config.input_path), optarg) != 0) {
                fprintf(stderr, "ERROR: input path is too long\n");
                return 2;
            }
            break;
        case 'r':
            if (copy_text(config.upimulator_root, sizeof(config.upimulator_root), optarg) != 0) {
                fprintf(stderr, "ERROR: uPIMulator path is too long\n");
                return 2;
            }
            break;
        case 'D':
            describe = 1;
            break;
        case 'h':
            print_usage(stdout, argv[0]);
            return 0;
        default:
            print_usage(stderr, argv[0]);
            return 2;
        }
    }

    if (describe) {
        if (optind != argc || config.workload_path[0] == '\0') {
            print_usage(stderr, argv[0]);
            return 2;
        }
        if (describe_workload(
                config.workload_path,
                error,
                sizeof(error)) != 0) {
            fprintf(stderr, "ERROR: %s\n", error);
            return 2;
        }
        return 0;
    }

    if (optind != argc || !config.mode_set || config.workload_path[0] == '\0' ||
        config.input_path[0] == '\0') {
        print_usage(stderr, argv[0]);
        return 2;
    }
    if (config.mode == MODE_HYBRID && !config.cpu_share_set) {
        fprintf(stderr, "ERROR: --cpu-share is required in hybrid mode\n");
        return 2;
    }
    if (config.mode != MODE_HYBRID && config.cpu_share_set) {
        fprintf(stderr, "ERROR: --cpu-share is only valid in hybrid mode\n");
        return 2;
    }

    if (engine_execute(&config, &result, error, sizeof(error)) != 0) {
        fprintf(stderr, "ERROR: %s\n", error);
        return 2;
    }

    printf("Workload: %s\n", result.workload_name);
    printf("Total elements: %zu\n", result.total_elements);
    printf("Execution mode: %s\n", execution_mode_name(config.mode));
    if (config.mode == MODE_HYBRID) {
        printf("CPU share: %.12Lg\n", config.cpu_share);
    }
    printf("DPU memory: %s\n", config.dpu_memory == DPU_MEMORY_MRAM ? "MRAM" : "WRAM");
    printf("DPU tasklets: %u\n\n", config.tasklets);
    printf("CPU elements: %zu\n", result.cpu_elements);
    printf("DPU elements: %zu\n\n", result.dpu_elements);
    printf("CPU partial result: %" PRIu32 "\n", result.cpu_partial_result);
    printf("DPU partial result: %" PRIu32 "\n", result.dpu_partial_result);
    if (result.dpu_ran) {
        printf("DPU simulated logic cycles: %" PRIu64 "\n", result.dpu_simulated_cycles);
    } else {
        printf("DPU simulated logic cycles: not run\n");
    }
    printf("\nCombined result: %" PRIu32 "\n", result.combined_result);
    printf("Reference result: %" PRIu32 "\n", result.reference_result);
    printf("Correctness: %s\n", result.correct ? "PASS" : "FAIL");
    return result.correct ? 0 : 1;
}
