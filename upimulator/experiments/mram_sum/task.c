#include <defs.h>
#include <mram.h>
#include <stdint.h>

#define BLOCK_ELEMENTS 16

__host uint32_t array_length;
__host uint32_t output_sum;

int main(void)
{
    __dma_aligned uint32_t cache[BLOCK_ELEMENTS];
    uint32_t mram_base = (uint32_t)DPU_MRAM_HEAP_POINTER;
    uint32_t sum = 0;

    for (uint32_t base = 0; base < array_length; base += BLOCK_ELEMENTS) {
        uint32_t block_length = array_length - base;
        if (block_length > BLOCK_ELEMENTS) {
            block_length = BLOCK_ELEMENTS;
        }

        mram_read(
            (__mram_ptr void const *)(mram_base + base * sizeof(uint32_t)),
            cache,
            sizeof(cache));

        for (uint32_t i = 0; i < block_length; i++) {
            sum += cache[i];
        }
    }

    output_sum = sum;
    return 0;
}
