#include <defs.h>
#include <stdint.h>

__host uint32_t input_value;
__host uint32_t iterations;
__host uint32_t output_value;

int main(void)
{
    volatile uint32_t value = input_value;

    for (uint32_t i = 0; i < iterations; i++) {
        value += 1;
    }

    output_value = value;

    return 0;
}
