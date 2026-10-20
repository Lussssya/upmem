# UPMEM SDK experiments

These experiments use the UPMEM SDK host/DPU programming model:

1. `increment` — transfer one value to a DPU and read the incremented result.
2. `sum_dpus` — split an array across multiple DPUs and combine partial sums.
3. `tasklets` — use multiple tasklets, shared partial results, and a barrier.
4. `cooperative_sum` — overlap host CPU and asynchronous DPU computation.
5. `mram_sum` — transfer MRAM blocks into WRAM with `mram_read()` before summing.

Each experiment contains `host.c`, `dpu.c`, a `Makefile`, and its own README.

```bash
cd increment
make clean
make
make run
```
