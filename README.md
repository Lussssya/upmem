# Processing-in-Memory Capstone Experiments

This repository is not just a collection of learning examples. It contains the
experiments for a Processing-in-Memory capstone project, covering both the UPMEM
SDK programming model and simulated cycle studies with uPIMulator.

## Repository contents

### UPMEM SDK experiments

The top-level C examples exercise the regular UPMEM host/DPU programming flow:

1. `increment` — transfer one value to a DPU and read the incremented result.
2. `sum_dpus` — split an array across multiple DPUs and combine partial sums.
3. `tasklets` — use multiple tasklets, shared partial results, and a barrier.
4. `cooperative_sum` — overlap host CPU and asynchronous DPU computation.
5. `mram_sum` — transfer MRAM blocks into WRAM with `mram_read()` before summing.

Each directory contains `host.c`, `dpu.c`, a `Makefile`, and its own README.

```bash
cd increment
make clean
make
make run
```

### uPIMulator experiments

The [`upimulator/`](upimulator/) directory contains controlled, single-DPU
experiments for studying simulated DPU cycles:

- `increment`
- `loop_add`
- `branch_work`
- `wram_sum`
- `mram_sum`
- `tasklet_sum`

uPIMulator models the host, channel, rank, DPU, tasklet scheduler, instruction
pipeline, WRAM, MRAM, DMA, and memory controller. These experiments use its
existing `Logic[0_0_0]_logic_cycle` statistic—not host wall-clock time—and keep
the important files easy to distinguish:

```text
run_<experiment>.sh  experiment runner and cycle summary
task.c               DPU kernel
host.go              input generation and correctness check
CMakeLists.txt        benchmark build integration
```

The included installer adds the experiments and portable native-build support
to the tested uPIMulator revision:

```bash
git clone https://github.com/VIA-Research/uPIMulator.git
git -C uPIMulator checkout 870d916334e9ff0b190f555f951a9ec3c4257781
./upimulator/install.sh ./uPIMulator/golang/uPIMulator

cd uPIMulator/golang/uPIMulator
./run_loop_add.sh 100
./run_wram_sum.sh 128
./run_mram_sum.sh 128
./run_tasklet_sum.sh 12
```

See [`upimulator/README.md`](upimulator/README.md) for prerequisites, complete
installation steps, experiment parameters, and all run commands.
