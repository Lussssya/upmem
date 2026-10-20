# Tasklet-sum experiment

This experiment sums the fixed 256-element WRAM array `1..256` on one DPU.
Elements are distributed cyclically across the requested number of tasklets;
tasklet 0 combines their partial sums after a barrier. The expected result is
always `32,896`, so tasklet count is the only varying parameter.

```bash
./run_tasklet_sum.sh 1
./run_tasklet_sum.sh 2
./run_tasklet_sum.sh 3
./run_tasklet_sum.sh 4
./run_tasklet_sum.sh 8
./run_tasklet_sum.sh 12
./run_tasklet_sum.sh 16
./run_tasklet_sum.sh 24
```

Successful output ends with:

```text
PASS: final sum is 32896.
SUMMARY: elements=256 tasklets=4 final_sum=32896 total_simulated_cycles=<cycles>
```

Files:

- `run_tasklet_sum.sh`: builds, runs, checks, and prints cycles.
- `task.c`: multi-tasklet WRAM kernel and barrier reduction.
- `host.go`: prepares the fixed array and expected sum.
- `CMakeLists.txt` and `dpu.CMakeLists.txt`: kernel build integration.

Run the commands from the uPIMulator root after using `install.sh`. Raw
statistics are written to `bin/tasklet_sum/log.txt`.

Every integer tasklet count from `1` through uPIMulator's architectural maximum
of `24` is accepted by the runner.
