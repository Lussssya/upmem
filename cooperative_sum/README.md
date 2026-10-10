# Cooperative CPU and DPU array sum

This example sums one logical 64-element array cooperatively. The CPU handles
elements 0 through 31 while one simulated DPU handles elements 32 through 63
with one tasklet.

The host copies only the DPU half, starts the DPU with `DPU_ASYNCHRONOUS`, and
then immediately computes the CPU half. After its own work, the host calls
`dpu_sync` before reading the DPU result. It adds the two partial sums and
checks the result against a normal full-array CPU sum.

```text
copy A[32..63] to DPU
  -> launch DPU asynchronously
  -> CPU sums A[0..31] while the DPU sums its half
  -> synchronize with the DPU
  -> read and combine both partial sums
  -> verify against the full CPU sum
```

Build and run:

```sh
make clean
make
make run
```

This is a correctness example using `backend=simulator`, not a performance
benchmark. Simulator timing should not be used to claim a speedup.
