#include <defs.h>
#include <stdint.h>
#include <stdio.h>

__host uint32_t input_value;
__host uint32_t output_value;

int main (void) {
    output_value = input_value + 1;
    printf("DPU: received %u, returning %u\n", input_value, output_value);
    return 0;
}
