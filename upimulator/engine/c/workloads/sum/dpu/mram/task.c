#include <barrier.h>
#include <defs.h>
#include <mram.h>
#include <stdint.h>

#define BLOCK_ELEMENTS 2

__host uint32_t array_length;
__host uint32_t output_value;

uint32_t partial_results[NR_TASKLETS];
BARRIER_INIT(completion_barrier, NR_TASKLETS);

int main(void)
{
    uint32_t tasklet_id = me();
    uint32_t local_result = 0;
    uint32_t mram_base = (uint32_t)DPU_MRAM_HEAP_POINTER;
    __dma_aligned uint32_t cache[BLOCK_ELEMENTS];

    for (uint32_t base = tasklet_id * BLOCK_ELEMENTS;
         base < array_length;
         base += NR_TASKLETS * BLOCK_ELEMENTS) {
        uint32_t block_length = array_length - base;
        if (block_length > BLOCK_ELEMENTS) {
            block_length = BLOCK_ELEMENTS;
        }
        mram_read(
            (__mram_ptr void const *)(mram_base + base * sizeof(uint32_t)),
            cache,
            sizeof(cache));
        for (uint32_t i = 0; i < block_length; i++) {
            local_result += cache[i];
        }
    }

    partial_results[tasklet_id] = local_result;
    barrier_wait(&completion_barrier);

    if (tasklet_id == 0) {
        uint32_t total = 0;
        for (uint32_t i = 0; i < NR_TASKLETS; i++) {
            total += partial_results[i];
        }
        output_value = total;
    }
    return 0;
}
