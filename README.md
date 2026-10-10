# UPMEM learning examples

These examples introduce the UPMEM programming model one step at a time:

1. `increment` — send one value to one simulated DPU and receive the result.
2. `sum_dpus` — split an array across multiple simulated DPUs and combine their partial sums.
3. `tasklets` — add multiple tasklets per DPU, shared partial results, and barrier synchronization.
4. `cooperative_sum` — overlap host CPU work with an asynchronous DPU computation, then synchronize and combine both results.

Progression: `increment` → `sum_dpus` → `tasklets` → `cooperative_sum`.

Each directory contains its own README and Makefile. Run an example with:

```sh
cd <example>
make clean
make
make run
```
