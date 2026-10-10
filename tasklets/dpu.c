#include <barrier.h>
#include <defs.h>
#include <stdint.h>
#include <stdio.h>

#define CHUNK_SIZE 16
// NR_TASKLETS is a UPMEM-defined compile-time value
#define ELEMENTS_PER_TASKLET (CHUNK_SIZE / NR_TASKLETS)

// __host makes these two symbols directly accessible to the host program
__host int32_t chunk[CHUNK_SIZE];
__host int64_t partial_sum;

// A file-scope WRAM array is shared by all tasklets on this DPU
int64_t tasklet_partial_sums[NR_TASKLETS];

BARRIER_INIT(sum_barrier, NR_TASKLETS);

int main (void) {
    // id of the current tasklet
    uint32_t tasklet_id = me();
    uint32_t start = tasklet_id * ELEMENTS_PER_TASKLET;
    uint32_t end = start + ELEMENTS_PER_TASKLET;
    int64_t local_sum = 0;

    for (uint32_t i = start; i < end; i++) {
        local_sum += chunk[i];
    }
    tasklet_partial_sums[tasklet_id] = local_sum;

    // Tasklet 0 cannot combine results until all four slots have been written
    barrier_wait(&sum_barrier);

    if (tasklet_id == 0) {
        int64_t dpu_sum = 0;

        for (uint32_t i = 0; i < NR_TASKLETS; i++) {
            dpu_sum += tasklet_partial_sums[i];
        }
        partial_sum = dpu_sum;

	printf("DPU: %u tasklets, %u elements per tasklet\n", NR_TASKLETS, ELEMENTS_PER_TASKLET);
    }

    return 0;
}
