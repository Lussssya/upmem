#define _POSIX_C_SOURCE 200809L

#include "dpu_backend.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int make_path(
    char *destination,
    size_t capacity,
    const char *root,
    const char *suffix,
    char *error,
    size_t error_capacity)
{
    int length = snprintf(destination, capacity, "%s/%s", root, suffix);
    if (length < 0 || (size_t)length >= capacity) {
        snprintf(error, error_capacity, "uPIMulator path is too long");
        return -1;
    }
    return 0;
}

static int make_values_csv(
    const uint32_t *values,
    size_t count,
    char *csv,
    size_t capacity,
    char *error,
    size_t error_capacity)
{
    size_t used = 0;

    for (size_t i = 0; i < count; i++) {
        int written = snprintf(
            csv + used,
            capacity - used,
            i == 0 ? "%" PRIu32 : ",%" PRIu32,
            values[i]);
        if (written < 0 || (size_t)written >= capacity - used) {
            snprintf(error, error_capacity, "DPU input argument is too long");
            return -1;
        }
        used += (size_t)written;
    }
    return 0;
}

static int run_simulator(
    const char *runner,
    const char *kernel_source,
    const char *output_tag,
    const char *memory,
    const char *tasklets,
    const char *expected_result,
    const char *values_csv,
    char *error,
    size_t error_capacity)
{
    char log_template[] = "/tmp/upimulator-engine-XXXXXX";
    int log_fd = mkstemp(log_template);
    posix_spawn_file_actions_t file_actions;
    char *const arguments[] = {
        (char *)runner,
        "--kernel-source",
        (char *)kernel_source,
        "--output-tag",
        (char *)output_tag,
        "--memory",
        (char *)memory,
        "--tasklets",
        (char *)tasklets,
        "--expected",
        (char *)expected_result,
        "--values",
        (char *)values_csv,
        NULL,
    };
    pid_t child;
    int actions_initialized = 0;
    int action_status;
    int spawn_status;
    int status;

    if (log_fd < 0) {
        snprintf(error, error_capacity, "cannot create simulator output file: %s", strerror(errno));
        return -1;
    }
    if (log_fd <= STDERR_FILENO) {
        int replacement = fcntl(log_fd, F_DUPFD, STDERR_FILENO + 1);

        if (replacement < 0) {
            snprintf(
                error,
                error_capacity,
                "cannot prepare simulator output file: %s",
                strerror(errno));
            close(log_fd);
            unlink(log_template);
            return -1;
        }
        close(log_fd);
        log_fd = replacement;
    }

    action_status = posix_spawn_file_actions_init(&file_actions);
    if (action_status == 0) {
        actions_initialized = 1;
        action_status = posix_spawn_file_actions_adddup2(
            &file_actions,
            log_fd,
            STDOUT_FILENO);
    }
    if (action_status == 0) {
        action_status = posix_spawn_file_actions_adddup2(
            &file_actions,
            log_fd,
            STDERR_FILENO);
    }
    if (action_status == 0) {
        action_status = posix_spawn_file_actions_addclose(&file_actions, log_fd);
    }
    if (action_status != 0) {
        snprintf(
            error,
            error_capacity,
            "cannot start uPIMulator: %s",
            strerror(action_status));
        if (actions_initialized) {
            posix_spawn_file_actions_destroy(&file_actions);
        }
        close(log_fd);
        unlink(log_template);
        return -1;
    }
    spawn_status = posix_spawn(
        &child,
        runner,
        &file_actions,
        NULL,
        arguments,
        environ);
    posix_spawn_file_actions_destroy(&file_actions);
    close(log_fd);
    if (spawn_status != 0) {
        snprintf(
            error,
            error_capacity,
            "cannot start uPIMulator: %s",
            strerror(spawn_status));
        unlink(log_template);
        return -1;
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            snprintf(error, error_capacity, "failed waiting for uPIMulator: %s", strerror(errno));
            unlink(log_template);
            return -1;
        }
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        FILE *log = fopen(log_template, "r");
        char details[2048] = "";

        if (log != NULL) {
            size_t bytes = fread(details, 1, sizeof(details) - 1, log);
            details[bytes] = '\0';
            fclose(log);
        }
        snprintf(
            error,
            error_capacity,
            "uPIMulator execution failed%s%s",
            details[0] == '\0' ? "" : ":\n",
            details);
        unlink(log_template);
        return -1;
    }

    unlink(log_template);
    return 0;
}

static int remove_stale_file(
    const char *path,
    const char *description,
    char *error,
    size_t error_capacity)
{
    if (unlink(path) == 0 || errno == ENOENT) {
        return 0;
    }

    snprintf(
        error,
        error_capacity,
        "cannot remove stale %s %s: %s",
        description,
        path,
        strerror(errno));
    return -1;
}

static int read_actual_output(
    const char *path,
    uint32_t *value,
    char *error,
    size_t error_capacity)
{
    FILE *file = fopen(path, "r");
    uint32_t bytes[4];
    char *line = NULL;
    size_t line_capacity = 0;

    if (file == NULL) {
        snprintf(
            error,
            error_capacity,
            "missing freshly captured DPU output %s",
            path);
        return -1;
    }

    for (size_t index = 0; index < 4; index++) {
        char *cursor;
        char *end = NULL;
        unsigned long parsed;

        if (getline(&line, &line_capacity, file) < 0) {
            if (ferror(file)) {
                snprintf(error, error_capacity, "failed reading captured DPU output %s", path);
            } else {
                snprintf(
                    error,
                    error_capacity,
                    "captured DPU output has %zu byte values; expected exactly 4",
                    index);
            }
            free(line);
            fclose(file);
            return -1;
        }

        cursor = line;
        while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (!isdigit((unsigned char)*cursor)) {
            snprintf(
                error,
                error_capacity,
                "captured DPU output byte %zu is not an unsigned decimal byte",
                index + 1);
            free(line);
            fclose(file);
            return -1;
        }

        errno = 0;
        parsed = strtoul(cursor, &end, 10);
        if (errno == ERANGE || parsed > UINT8_MAX) {
            snprintf(
                error,
                error_capacity,
                "captured DPU output byte %zu is outside 0..255: %s",
                index + 1,
                cursor);
            free(line);
            fclose(file);
            return -1;
        }
        while (*end != '\0' && isspace((unsigned char)*end)) {
            end++;
        }
        if (*end != '\0') {
            snprintf(
                error,
                error_capacity,
                "captured DPU output byte %zu has unexpected trailing content",
                index + 1);
            free(line);
            fclose(file);
            return -1;
        }
        bytes[index] = (uint32_t)parsed;
    }

    while (getline(&line, &line_capacity, file) >= 0) {
        for (char *cursor = line; *cursor != '\0'; cursor++) {
            if (!isspace((unsigned char)*cursor)) {
                snprintf(
                    error,
                    error_capacity,
                    "captured DPU output has unexpected trailing content");
                free(line);
                fclose(file);
                return -1;
            }
        }
    }
    if (ferror(file)) {
        snprintf(error, error_capacity, "failed reading captured DPU output %s", path);
        free(line);
        fclose(file);
        return -1;
    }
    free(line);
    fclose(file);

    *value = bytes[0] |
        (bytes[1] << 8) |
        (bytes[2] << 16) |
        (bytes[3] << 24);
    return 0;
}

static int read_logic_cycles(
    const char *path,
    uint64_t *cycles,
    char *error,
    size_t error_capacity)
{
    static const char key[] = "Logic[0_0_0]_logic_cycle: ";
    FILE *file = fopen(path, "r");
    char *line = NULL;
    size_t capacity = 0;

    if (file == NULL) {
        snprintf(error, error_capacity, "missing uPIMulator statistics %s", path);
        return -1;
    }

    while (getline(&line, &capacity, file) >= 0) {
        if (strncmp(line, key, sizeof(key) - 1) == 0) {
            char *end = NULL;
            unsigned long long parsed;
            char *value = line + sizeof(key) - 1;

            if (!isdigit((unsigned char)*value)) {
                snprintf(error, error_capacity, "invalid logic-cycle value in %s", path);
                free(line);
                fclose(file);
                return -1;
            }
            errno = 0;
            parsed = strtoull(value, &end, 10);
            if (errno == ERANGE || parsed > UINT64_MAX) {
                snprintf(error, error_capacity, "logic-cycle value overflows uint64 in %s", path);
                free(line);
                fclose(file);
                return -1;
            }
            while (*end != '\0' && isspace((unsigned char)*end)) {
                end++;
            }
            if (*end != '\0') {
                snprintf(error, error_capacity, "invalid trailing content in logic-cycle value in %s", path);
                free(line);
                fclose(file);
                return -1;
            }

            *cycles = (uint64_t)parsed;
            free(line);
            fclose(file);
            return 0;
        }
    }

    if (ferror(file)) {
        snprintf(error, error_capacity, "failed reading uPIMulator statistics %s", path);
        free(line);
        fclose(file);
        return -1;
    }
    free(line);
    fclose(file);
    snprintf(error, error_capacity, "logic-cycle statistic was not found in %s", path);
    return -1;
}

const char *dpu_memory_name(DpuMemory memory)
{
    return memory == DPU_MEMORY_MRAM ? "mram" : "wram";
}

int dpu_backend_execute(
    const char *upimulator_root,
    const LoadedWorkload *workload,
    DpuMemory memory,
    unsigned int tasklets,
    const uint32_t *values,
    size_t count,
    uint32_t expected_result,
    BackendResult *result,
    char *error,
    size_t error_capacity)
{
    char runner[ENGINE_PATH_CAPACITY];
    char output_path[ENGINE_PATH_CAPACITY];
    char statistics_path[ENGINE_PATH_CAPACITY];
    char kernel_source[ENGINE_PATH_CAPACITY];
    char bin_suffix[256];
    char output_suffix[288];
    char statistics_suffix[288];
    char tasklet_text[16];
    char expected_text[16];
    char values_csv[ENGINE_MAX_ELEMENTS * 12 + 1];
    uint32_t actual_result;

    if (result == NULL || workload == NULL || workload->api == NULL) {
        snprintf(error, error_capacity, "DPU backend received a null workload or result");
        return -1;
    }
    result->partial_result = workload->api->identity;
    result->ran = false;
    result->simulated_cycles = 0;
    if (count == 0) {
        return 0;
    }
    if (upimulator_root == NULL || values == NULL) {
        snprintf(error, error_capacity, "DPU backend received a null input");
        return -1;
    }
    if (count > ENGINE_MAX_ELEMENTS) {
        snprintf(error, error_capacity, "DPU backend supports at most %d values", ENGINE_MAX_ELEMENTS);
        return -1;
    }
    if (memory != DPU_MEMORY_WRAM && memory != DPU_MEMORY_MRAM) {
        snprintf(error, error_capacity, "invalid DPU memory mode");
        return -1;
    }
    if (tasklets < 1 || tasklets > 24) {
        snprintf(error, error_capacity, "DPU tasklet count must be from 1 through 24");
        return -1;
    }

    if (workload->api->dpu_runner[0] == '/') {
        int runner_length = snprintf(runner, sizeof(runner), "%s", workload->api->dpu_runner);
        if (runner_length < 0 || (size_t)runner_length >= sizeof(runner)) {
            snprintf(error, error_capacity, "DPU runner path is invalid");
            return -1;
        }
    } else if (make_path(
                   runner,
                   sizeof(runner),
                   upimulator_root,
                   workload->api->dpu_runner,
                   error,
                   error_capacity) != 0) {
        return -1;
    }
    if (access(runner, X_OK) != 0) {
        snprintf(error, error_capacity, "DPU backend is not installed: missing %s", runner);
        return -1;
    }

    if (workload_resolve_kernel(
            workload,
            memory,
            kernel_source,
            sizeof(kernel_source),
            error,
            error_capacity) != 0) {
        return -1;
    }
    snprintf(
        bin_suffix,
        sizeof(bin_suffix),
        "bin/scalar_workload_%s_%s_%u",
        workload->api->workload_id,
        dpu_memory_name(memory),
        tasklets);
    if (snprintf(tasklet_text, sizeof(tasklet_text), "%u", tasklets) < 0 ||
        snprintf(expected_text, sizeof(expected_text), "%" PRIu32, expected_result) < 0 ||
        make_values_csv(values, count, values_csv, sizeof(values_csv), error, error_capacity) != 0 ||
        snprintf(output_suffix, sizeof(output_suffix), "%s/actual_output.bin", bin_suffix) < 0 ||
        snprintf(statistics_suffix, sizeof(statistics_suffix), "%s/log.txt", bin_suffix) < 0 ||
        make_path(output_path, sizeof(output_path), upimulator_root, output_suffix, error, error_capacity) != 0 ||
        make_path(statistics_path, sizeof(statistics_path), upimulator_root, statistics_suffix, error, error_capacity) != 0) {
        return -1;
    }

    if (remove_stale_file(
            output_path,
            "DPU output",
            error,
            error_capacity) != 0 ||
        remove_stale_file(
            statistics_path,
            "DPU statistics",
            error,
            error_capacity) != 0 ||
        run_simulator(
            runner,
            kernel_source,
            workload->api->workload_id,
            dpu_memory_name(memory),
            tasklet_text,
            expected_text,
            values_csv,
            error,
            error_capacity) != 0 ||
        read_actual_output(output_path, &actual_result, error, error_capacity) != 0 ||
        read_logic_cycles(statistics_path, &result->simulated_cycles, error, error_capacity) != 0) {
        return -1;
    }

    result->partial_result = actual_result;
    result->ran = true;
    return 0;
}
