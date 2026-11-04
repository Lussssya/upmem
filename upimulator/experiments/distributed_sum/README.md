# Distributed-sum DPU backend

This is the WRAM variant of the simulated-DPU backend for the CPU–DPU
distribution engine. It accepts arbitrary `uint32` values, distributes them
cyclically across 1–24 tasklets, and exposes the actual four-byte DPU result
captured during uPIMulator's output transfer.

Limits:

- 1 through 256 DPU elements;
- each input is from 0 through 4,294,967,295;
- the partial sum must fit in `uint32`.

After installing the experiments into uPIMulator:

```bash
./run_distributed_sum.sh '5,9,2,100,7'
./run_distributed_sum.sh --memory wram --tasklets 4 '5,9,2,100,7'
./run_distributed_sum.sh --memory mram --tasklets 8 '5,9,2,100,7'
```

The runner preserves uPIMulator's built-in expected-output comparison, but its
reported sum is decoded from `bin/distributed_sum/actual_output.bin`, which is
written from the simulated DPU output path.
