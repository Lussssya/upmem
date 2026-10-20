# WRAM-sum experiment

This controlled baseline sums the `uint32_t` values `1..N` directly from DPU
WRAM using one DPU and one tasklet. `N` may be from 1 through 256 and defaults
to 256 when omitted.

```bash
./run_wram_sum.sh 32
./run_wram_sum.sh 64
./run_wram_sum.sh 128
./run_wram_sum.sh 256
./run_wram_sum.sh      # defaults to 256
```

Successful output ends with:

```text
PASS: final sum is 32896.
SUMMARY: elements=256 memory=WRAM final_sum=32896 total_simulated_cycles=<cycles>
```

Files:

- `run_wram_sum.sh`: builds, runs, checks, and prints cycles.
- `task.c`: WRAM-based DPU kernel.
- `host.go`: prepares values `1..N` and the expected sum.
- `CMakeLists.txt` and `dpu.CMakeLists.txt`: kernel build integration.

Run the commands from the uPIMulator root after using `install.sh`. Raw
statistics are written to `bin/wram_sum/log.txt`.
