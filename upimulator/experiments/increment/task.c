#include <defs.h>
#include <stdint.h>

__host uint32_t input_value;
__host uint32_t output_value;

int main(void)
{
    output_value = input_value + 1;

    return 0;
}
