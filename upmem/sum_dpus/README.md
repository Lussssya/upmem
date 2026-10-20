# Sum an array with multiple simulated UPMEM DPUs

This example splits a fixed array of 64 integers evenly across 4 simulated
DPUs. Each DPU receives 16 integers, computes one partial sum with one tasklet,
and exposes that sum to the host. The host combines the four partial sums and
compares the result with a normal CPU sum.

The data flow is:

```text
host array
  -> split into four chunks
  -> copy one chunk to each DPU
  -> launch all DPUs synchronously
  -> copy one partial sum from each DPU
  -> add partial sums on the host
  -> compare with the CPU result
```

The example deliberately uses no barriers, mutexes, dynamic scheduling, or
asynchronous launch. Both allocation and the run target select
`backend=simulator`; no UPMEM hardware is required.

Build and run:

```sh
make clean
make
make run
```
