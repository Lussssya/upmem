#include <defs.h>
#include <stdint.h>

__host uint32_t branch_selector;
__host uint32_t output_value;

int main(void)
{
    uint32_t work;
    volatile uint32_t result = 0;

    if (branch_selector == 0) {
        work = 10;
    } else {
        work = 1000;
    }

    for (uint32_t i = 0; i < work; i++) {
        result += 1;
    }

    output_value = result;
    return 0;
}
