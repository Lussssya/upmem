#include <defs.h>
#include <stdint.h>

#define DPU_PART_SIZE 32

// __host makes these symbols directly accessible to the host program
__host int32_t dpu_input[DPU_PART_SIZE];
__host int64_t partial_sum;

int main (void) {
    int64_t local_sum = 0;

    for (int i = 0; i < DPU_PART_SIZE; i++) {
        local_sum += dpu_input[i];
    }

    partial_sum = local_sum;
    return 0;
}
