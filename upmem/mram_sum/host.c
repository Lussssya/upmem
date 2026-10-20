#include <dpu.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DPU_BINARY
#define DPU_BINARY "build/dpu"
#endif

#define NR_DPUS 1
#define ARRAY_SIZE 256

int main (void) {
    struct dpu_set_t set, dpu;
    int32_t input_array[ARRAY_SIZE];
    int64_t dpu_sum = 0;
    int64_t cpu_reference_sum = 0;
    uint32_t nr_dpus = 0;

    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_array[i] = (int32_t)(i + 1);
        cpu_reference_sum += input_array[i];
    }

    DPU_ASSERT(dpu_alloc(NR_DPUS, "backend=simulator", &set));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));

    if (nr_dpus != NR_DPUS) {
        fprintf(stderr, "FAIL: requested %u DPU but allocated %u\n",
                NR_DPUS, nr_dpus);
        DPU_ASSERT(dpu_free(set));
        return EXIT_FAILURE;
    }

    // Copy the full input array from host memory into the DPU's MRAM symbol
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_to(dpu, "mram_input", 0, input_array,
                               sizeof(input_array)));
    }

    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));

    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_from(dpu, "partial_sum", 0, &dpu_sum,
                                 sizeof(dpu_sum)));
    }

    DPU_ASSERT(dpu_free(set));

    printf("DPU sum: %" PRId64 "\n", dpu_sum);
    printf("CPU reference sum: %" PRId64 "\n", cpu_reference_sum);

    if (dpu_sum != cpu_reference_sum) {
        puts("FAIL");
        return EXIT_FAILURE;
    }

    puts("PASS");
    return EXIT_SUCCESS;
}
