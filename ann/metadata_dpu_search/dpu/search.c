#include "search_common.h"

#include <defs.h>
#include <float.h>
#include <mram.h>
#include <stdint.h>

#define VECTOR_BYTES (VECTOR_DIMENSIONS * sizeof(float))

__mram_noinit float mram_query[VECTOR_DIMENSIONS];
__mram_noinit float mram_candidates[MAX_CANDIDATES][VECTOR_DIMENSIONS];
__mram_noinit uint64_t mram_candidate_ids[MAX_CANDIDATES];

__host search_config_t search_config;
__host search_output_t search_output;

__dma_aligned float query_buffer[VECTOR_DIMENSIONS];
__dma_aligned float candidate_buffer[VECTOR_DIMENSIONS];
__dma_aligned uint64_t id_buffer[1];

_Static_assert(VECTOR_BYTES == 512U, "SIFT vector transfer must be 512 bytes");
_Static_assert(sizeof(search_config_t) == 8U, "configuration must be DMA aligned");
_Static_assert(sizeof(search_output_t) == 48U, "output must be DMA aligned");

static int is_better(float distance, uint32_t id, uint32_t position) {
    return distance < search_output.distances[position] ||
           (distance == search_output.distances[position] &&
            id < search_output.ids[position]);
}

static void insert_neighbor(float distance, uint32_t id, uint32_t k) {
    uint32_t position = 0;
    while (position < k && !is_better(distance, id, position)) {
        ++position;
    }
    if (position == k) {
        return;
    }

    for (uint32_t current = k - 1U; current > position; --current) {
        search_output.distances[current] = search_output.distances[current - 1U];
        search_output.ids[current] = search_output.ids[current - 1U];
    }
    search_output.distances[position] = distance;
    search_output.ids[position] = id;
}

int main(void) {
    search_output.result_count = 0U;
    search_output.status = SEARCH_STATUS_OK;
    for (uint32_t rank = 0; rank < MAX_K; ++rank) {
        search_output.ids[rank] = UINT32_MAX;
        search_output.distances[rank] = FLT_MAX;
    }

    if (search_config.candidate_count > MAX_CANDIDATES ||
        search_config.k == 0U || search_config.k > MAX_K) {
        search_output.status = SEARCH_STATUS_INVALID_CONFIG;
        return 0;
    }

    const uint32_t result_count =
        search_config.candidate_count < search_config.k
            ? search_config.candidate_count
            : search_config.k;
    search_output.result_count = result_count;
    if (result_count == 0U) {
        return 0;
    }

    mram_read(&mram_query[0], query_buffer, VECTOR_BYTES);

    for (uint32_t candidate = 0; candidate < search_config.candidate_count;
         ++candidate) {
        mram_read(&mram_candidates[candidate][0], candidate_buffer, VECTOR_BYTES);
        mram_read(&mram_candidate_ids[candidate], id_buffer, sizeof(id_buffer));
        if (id_buffer[0] > UINT32_MAX) {
            search_output.status = SEARCH_STATUS_INVALID_ID;
            return 0;
        }

        float distance = 0.0F;
        for (uint32_t dimension = 0; dimension < VECTOR_DIMENSIONS; ++dimension) {
            const float difference =
                query_buffer[dimension] - candidate_buffer[dimension];
            distance += difference * difference;
        }
        insert_neighbor(distance, (uint32_t)id_buffer[0], result_count);
    }
    return 0;
}

