#include <dpu.h>
#include <dpu_log.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DPU_BINARY
#define DPU_BINARY "build/dpu"
#endif

int main (void) {
    struct dpu_set_t set, dpu;
    uint32_t input = 41;
    uint32_t output = 0;
    uint32_t nr_dpus = 0;

    // Select the functional simulator explicitly (never request hardware)
    DPU_ASSERT(dpu_alloc(1, "backend=simulator", &set));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));

    printf("HOST: allocated %u simulated DPU\n", nr_dpus);
    printf("HOST: sending %u to DPU\n", input);
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_to(dpu, "input_value", 0, &input, sizeof(input)));
    }
    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));

    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_from(dpu, "output_value", 0, &output, sizeof(output)));
        DPU_ASSERT(dpu_log_read(dpu, stdout));
    }

    printf("HOST: received %u from DPU\n", output);
    DPU_ASSERT(dpu_free(set));

    if (nr_dpus != 1 || output != input + 1) {
        fprintf(stderr, "FAIL: host/DPU round trip returned an unexpected value\n");
        return EXIT_FAILURE;
    }

    puts("PASS: host <-> simulated DPU round trip succeeded");
    return EXIT_SUCCESS;
}
