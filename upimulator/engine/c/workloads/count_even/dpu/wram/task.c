#include <barrier.h>
#include <defs.h>
#include <stdint.h>

#define ARRAY_CAPACITY 256

__host uint32_t array_length;
__host uint32_t input_values[ARRAY_CAPACITY];
__host uint32_t output_value;

uint32_t partial_results[NR_TASKLETS];
BARRIER_INIT(completion_barrier, NR_TASKLETS);

int main(void)
{
    uint32_t tasklet_id = me();
    uint32_t local_count = 0;

    for (uint32_t i = tasklet_id; i < array_length; i += NR_TASKLETS) {
        if ((input_values[i] & 1u) == 0u) {
            local_count++;
        }
    }

    partial_results[tasklet_id] = local_count;
    barrier_wait(&completion_barrier);

    if (tasklet_id == 0) {
        uint32_t total_count = 0;

        for (uint32_t i = 0; i < NR_TASKLETS; i++) {
            total_count += partial_results[i];
        }
        output_value = total_count;
    }
    return 0;
}
