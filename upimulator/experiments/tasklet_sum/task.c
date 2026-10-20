#include <barrier.h>
#include <defs.h>
#include <stdint.h>

#define ARRAY_CAPACITY 256

__host uint32_t array_length;
__host uint32_t input_values[ARRAY_CAPACITY];
__host uint32_t output_sum;

uint32_t partial_sums[NR_TASKLETS];
BARRIER_INIT(completion_barrier, NR_TASKLETS);

int main(void)
{
    uint32_t tasklet_id = me();
    uint32_t local_sum = 0;

    for (uint32_t i = tasklet_id; i < array_length; i += NR_TASKLETS) {
        local_sum += input_values[i];
    }

    partial_sums[tasklet_id] = local_sum;
    barrier_wait(&completion_barrier);

    if (tasklet_id == 0) {
        uint32_t total = 0;

        for (uint32_t i = 0; i < NR_TASKLETS; i++) {
            total += partial_sums[i];
        }

        output_sum = total;
    }

    return 0;
}
