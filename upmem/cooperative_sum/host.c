#include <dpu.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DPU_BINARY
#define DPU_BINARY "build/dpu"
#endif

#define NR_DPUS 1
#define ARRAY_SIZE 64
#define CPU_PART_SIZE 32
#define DPU_PART_SIZE 32

int main (void) {
    struct dpu_set_t set, dpu;
    int32_t input_array[ARRAY_SIZE];
    int64_t cpu_partial_sum = 0;
    int64_t dpu_partial_sum = 0;
    int64_t cooperative_total = 0;
    int64_t reference_cpu_total = 0;
    uint32_t nr_dpus = 0;

    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_array[i] = (int32_t)(i + 1);
        reference_cpu_total += input_array[i];
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

    // Transfer only the second half; the first half remains CPU work
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_to(dpu, "dpu_input", 0,
                               &input_array[CPU_PART_SIZE],
                               DPU_PART_SIZE * sizeof(input_array[0])));
    }

    // This returns after the DPU starts, allowing host work to overlap it
    DPU_ASSERT(dpu_launch(set, DPU_ASYNCHRONOUS));

    for (int i = 0; i < CPU_PART_SIZE; i++) {
        cpu_partial_sum += input_array[i];
    }

    // Wait before accessing the result produced by the asynchronous DPU run
    DPU_ASSERT(dpu_sync(set));
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_from(dpu, "partial_sum", 0, &dpu_partial_sum,
                                 sizeof(dpu_partial_sum)));
    }

    cooperative_total = cpu_partial_sum + dpu_partial_sum;

    printf("CPU partial sum: %" PRId64 "\n", cpu_partial_sum);
    printf("DPU partial sum: %" PRId64 "\n", dpu_partial_sum);
    printf("Cooperative total: %" PRId64 "\n", cooperative_total);
    printf("Reference CPU total: %" PRId64 "\n", reference_cpu_total);

    DPU_ASSERT(dpu_free(set));

    if (cooperative_total != reference_cpu_total) {
        puts("FAIL");
        return EXIT_FAILURE;
    }

    puts("PASS");
    return EXIT_SUCCESS;
}
