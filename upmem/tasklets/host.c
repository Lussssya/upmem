#include <dpu.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dpu_log.h>

#ifndef DPU_BINARY
#define DPU_BINARY "build/dpu"
#endif

#define NR_DPUS 4
#define ARRAY_SIZE 64
#define CHUNK_SIZE (ARRAY_SIZE / NR_DPUS)

int main (void) {
    struct dpu_set_t set, dpu;
    int32_t input_array[ARRAY_SIZE];
    int64_t dpu_total_sum = 0;
    int64_t cpu_total_sum = 0;
    uint32_t nr_dpus = 0;
    uint32_t dpu_index;

    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_array[i] = (int32_t)(i + 1);
        cpu_total_sum += input_array[i];
    }

    // Allocate four simulated DPUs and load the same four-tasklet program on each
    DPU_ASSERT(dpu_alloc(NR_DPUS, "backend=simulator", &set));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));

    if (nr_dpus != NR_DPUS) {
        fprintf(stderr, "FAIL: requested %u DPUs but allocated %u\n", NR_DPUS, nr_dpus);
        DPU_ASSERT(dpu_free(set));
        return EXIT_FAILURE;
    }

    printf("Using %u simulated DPUs\n",
           nr_dpus);
    printf("Array size: %u; chunk size: %u per DPU\n",
           ARRAY_SIZE, CHUNK_SIZE);

    // Each DPU receives one different 16-element chunk in its own chunk symbol
    DPU_FOREACH(set, dpu, dpu_index) {
        int32_t *chunk = &input_array[dpu_index * CHUNK_SIZE];
        DPU_ASSERT(dpu_copy_to(dpu, "chunk", 0, chunk, CHUNK_SIZE * sizeof(*chunk)));
    }

    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));

    // Read one combined result from each DPU and finish the reduction on the host
    DPU_FOREACH(set, dpu, dpu_index) {
        int64_t partial_sum = 0;
        DPU_ASSERT(dpu_copy_from(dpu, "partial_sum", 0, &partial_sum, sizeof(partial_sum)));
	DPU_ASSERT(dpu_log_read(dpu, stdout));
	printf("DPU %u partial sum: %" PRId64 "\n", dpu_index, partial_sum);
        dpu_total_sum += partial_sum;
    }

    DPU_ASSERT(dpu_free(set));

    printf("DPU total: %" PRId64 "\n", dpu_total_sum);
    printf("CPU total: %" PRId64 "\n", cpu_total_sum);

    if (dpu_total_sum != cpu_total_sum) {
        puts("FAIL");
        return EXIT_FAILURE;
    }

    puts("PASS");
    return EXIT_SUCCESS;
}
