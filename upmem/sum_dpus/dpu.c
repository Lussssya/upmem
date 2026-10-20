#include <defs.h>
#include <stdint.h>

#define CHUNK_SIZE 16

// __host makes these symbols directly accessible to the host program.
__host int32_t chunk[CHUNK_SIZE];
__host int64_t partial_sum;

int main (void) {
    int64_t local_sum = 0;

    for (int i = 0; i < CHUNK_SIZE; i++) {
        local_sum += chunk[i];
    }

    partial_sum = local_sum;
    return 0;
}
