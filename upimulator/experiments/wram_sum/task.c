#include <defs.h>
#include <stdint.h>

#define ARRAY_CAPACITY 256

__host uint32_t array_length;
__host uint32_t input_values[ARRAY_CAPACITY];
__host uint32_t output_sum;

int main(void)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < array_length; i++) {
        sum += input_values[i];
    }

    output_sum = sum;
    return 0;
}
