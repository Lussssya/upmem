# uPIMulator cycle experiments

These experiments run UPMEM DPU programs inside the Go implementation of
[uPIMulator](https://github.com/VIA-Research/uPIMulator) and report simulated
DPU logic cycles. They do not use host wall-clock time.

## Experiment files

Every directory under `experiments/` keeps the important parts separate:

- `run_<name>.sh` builds, runs, checks correctness, and prints simulated cycles.
- `task.c` is the program executed by the simulated DPU.
- `host.go` prepares DPU inputs and the expected output for correctness checking.
- `CMakeLists.txt` and `dpu.CMakeLists.txt` integrate the DPU program into the build.
- `README.md` describes the parameter being measured.

## Experiments

| Experiment | Variable | Purpose |
| --- | --- | --- |
| `increment` | input value | Small correctness and cycle-reporting smoke test |
| `loop_add` | loop iterations | Cycle scaling as computation increases |
| `branch_work` | selected branch | Compare short and long execution paths |
| `wram_sum` | elements, 1–256 | WRAM summation baseline |
| `mram_sum` | elements, 1–256 | Explicit MRAM-to-WRAM block transfers |
| `tasklet_sum` | tasklets, 1–24 | Intra-DPU parallelism and synchronization overhead |

All experiments use one simulated DPU. Only `tasklet_sum` changes the tasklet
count.

## Requirements

- Linux
- UPMEM SDK with `dpu-upmem-dpurte-clang`
- Go
- Python 3
- CMake and Ninja
- uPIMulator commit `870d916334e9ff0b190f555f951a9ec3c4257781`

## Install into uPIMulator

```bash
git clone https://github.com/VIA-Research/uPIMulator.git
git -C uPIMulator checkout 870d916334e9ff0b190f555f951a9ec3c4257781

git clone https://github.com/Lussssya/upmem.git
./upmem/upimulator/install.sh ./uPIMulator/golang/uPIMulator
```

Either source the UPMEM SDK first or set `UPMEM_SDK_DIR` to its installation
directory. For example:

```bash
export UPMEM_SDK_DIR="$HOME/pim/upmem-sdk"
```

## Run

```bash
cd uPIMulator/golang/uPIMulator

./run_increment.sh 41
./run_loop_add.sh 100
./run_branch_work.sh 0
./run_branch_work.sh 1
./run_wram_sum.sh 128
./run_mram_sum.sh 128
./run_tasklet_sum.sh 12
```

Successful runs end with `PASS` and a `SUMMARY`. The cycle value is read from
`Logic[0_0_0]_logic_cycle` in `bin/<experiment>/log.txt`.

## Portable native-build overlay

The `native/` directory contains the shared uPIMulator changes needed to build
with the locally installed UPMEM compiler instead of the original Docker-only
flow. `install.sh` copies this overlay and registers each `host.go` adapter.
