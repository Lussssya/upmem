#ifndef METADATA_DPU_SEARCH_COMMON_H
#define METADATA_DPU_SEARCH_COMMON_H

#include <stdint.h>

#define VECTOR_DIMENSIONS 128U
#define MAX_CANDIDATES 10000U
#define MAX_K 5U

#define SEARCH_STATUS_OK 0U
#define SEARCH_STATUS_INVALID_CONFIG 1U
#define SEARCH_STATUS_INVALID_ID 2U

typedef struct {
    uint32_t candidate_count;
    uint32_t k;
} search_config_t;

typedef struct {
    uint32_t result_count;
    uint32_t status;
    uint32_t ids[MAX_K];
    float distances[MAX_K];
} search_output_t;

#endif

