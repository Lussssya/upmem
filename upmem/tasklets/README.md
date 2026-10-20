# Sum an array with multiple DPUs and tasklets

This example sums 64 integers with 4 simulated UPMEM DPUs and 4 tasklets on
each DPU. The host sends 16 integers to every DPU. Inside a DPU, tasklet IDs
0 through 3 each sum a fixed range of 4 integers.

The DPU-side flow is:

```text
16-element DPU chunk
  -> tasklet 0 sums elements 0..3
  -> tasklet 1 sums elements 4..7
  -> tasklet 2 sums elements 8..11
  -> tasklet 3 sums elements 12..15
  -> all tasklets wait at one barrier
  -> tasklet 0 combines the four tasklet partial sums
  -> the host reads one partial_sum from the DPU
```

`me()` returns the current tasklet ID. Each tasklet writes to a different
element of the file-scope `tasklet_partial_sums` array, which is shared in DPU
WRAM. `BARRIER_INIT` and `barrier_wait` ensure every element is ready before
tasklet 0 performs the final DPU-local reduction.

The launch is synchronous and uses `backend=simulator`; no UPMEM hardware is
required. There are no mutexes, atomics, dynamic scheduling, or asynchronous
operations.

Build and run:

```sh
make clean
make
make run
```
