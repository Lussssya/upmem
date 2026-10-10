#include <defs.h>
#include <mram.h>
#include <stdint.h>

#define ARRAY_SIZE 256
#define BLOCK_SIZE 32
#define BLOCK_BYTES (BLOCK_SIZE * sizeof(int32_t))

__mram_noinit int32_t mram_input[ARRAY_SIZE];
__host int64_t partial_sum;

// This DMA-aligned array is the small working buffer in WRAM
__dma_aligned int32_t wram_buffer[BLOCK_SIZE];

int main (void) {
    int64_t local_sum = 0;

    for (int block_start = 0; block_start < ARRAY_SIZE;
         block_start += BLOCK_SIZE) {
        // Explicitly move one 128-byte block from MRAM into WRAM
        mram_read(&mram_input[block_start], wram_buffer, BLOCK_BYTES);

        // Arithmetic uses the WRAM copy, not the MRAM array directly
        for (int i = 0; i < BLOCK_SIZE; i++) {
            local_sum += wram_buffer[i];
        }
    }

    partial_sum = local_sum;
    return 0;
}
