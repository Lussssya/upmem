# Branch-work experiment

This single-DPU, single-tasklet experiment compares two execution paths.
Selector `0` performs 10 additions; any nonzero selector performs 1,000.

```bash
./run_branch_work.sh 0
./run_branch_work.sh 1
```

Successful runs print the selected path, final result, and total simulated
cycles.

- `run_branch_work.sh`: builds, runs, checks, and prints cycles.
- `task.c`: DPU kernel containing the branch and work loops.
- `host.go`: prepares the selector and expected result.
- `CMakeLists.txt` and `dpu.CMakeLists.txt`: kernel build integration.

Run the commands from the uPIMulator root after using `install.sh`. Raw
statistics are written to `bin/branch_work/log.txt`.
