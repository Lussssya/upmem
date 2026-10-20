# Loop-add cycle experiment

This experiment measures how uPIMulator's simulated DPU logic-cycle count
changes as a simple computation loop grows.

The test uses one simulated DPU and one tasklet. It starts at `41`, adds one
for the requested number of iterations, checks that the final result equals
`41 + iterations`, and reports `Logic[0_0_0]_logic_cycle` from uPIMulator's
existing `log.txt` statistics.

## Run

```bash
./run_loop_add.sh 1
./run_loop_add.sh 10
./run_loop_add.sh 100
./run_loop_add.sh 1000
```

Each successful run ends with output in this form:

```text
PASS: final result is 51.
SUMMARY: iterations=10 final_result=51 total_simulated_cycles=<cycles>
```

## Files

- `run_loop_add.sh`: builds, runs, checks, and prints cycles.
- `task.c`: UPMEM-C kernel executed by the DPU.
- `host.go`: prepares inputs and the expected result.
- `CMakeLists.txt` and `dpu.CMakeLists.txt`: kernel build integration.

Run the commands from the uPIMulator root after using `install.sh`. Raw
statistics are written to `bin/loop_add/log.txt`.
