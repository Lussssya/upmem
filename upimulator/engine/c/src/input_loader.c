#define _POSIX_C_SOURCE 200809L

#include "input_loader.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int input_load(
    const char *path,
    WorkloadInput *input,
    char *error,
    size_t error_capacity)
{
    FILE *file = fopen(path, "r");
    char *line = NULL;
    size_t line_capacity = 0;

    input->count = 0;
    if (file == NULL) {
        snprintf(error, error_capacity, "cannot open input %s: %s", path, strerror(errno));
        return -1;
    }

    while (getline(&line, &line_capacity, file) >= 0) {
        char *comment = strchr(line, '#');
        char *save_pointer = NULL;

        if (comment != NULL) {
            *comment = '\0';
        }

        for (char *token = strtok_r(line, ", \t\r\n", &save_pointer);
             token != NULL;
             token = strtok_r(NULL, ", \t\r\n", &save_pointer)) {
            char *end = NULL;
            unsigned long long parsed;

            if (input->count == ENGINE_MAX_ELEMENTS) {
                snprintf(error, error_capacity, "input supports at most %d values", ENGINE_MAX_ELEMENTS);
                free(line);
                fclose(file);
                return -1;
            }
            if (token[0] == '-' || token[0] == '+') {
                snprintf(error, error_capacity, "invalid uint32 value: %s", token);
                free(line);
                fclose(file);
                return -1;
            }

            errno = 0;
            parsed = strtoull(token, &end, 10);
            if (errno != 0 || end == token || *end != '\0' || parsed > UINT32_MAX) {
                snprintf(error, error_capacity, "invalid uint32 value: %s", token);
                free(line);
                fclose(file);
                return -1;
            }

            input->values[input->count++] = (uint32_t)parsed;
        }
    }

    if (ferror(file)) {
        snprintf(error, error_capacity, "failed while reading input %s", path);
        free(line);
        fclose(file);
        return -1;
    }

    free(line);
    fclose(file);
    if (input->count == 0) {
        snprintf(error, error_capacity, "input must contain at least one value");
        return -1;
    }
    return 0;
}
