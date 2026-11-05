# CPU–DPU distribution engine

The active implementation is the generic compiled C engine in [`c/`](c/). It
loads workload plugins at runtime and supports native CPU-only, simulated
DPU-only, and manually partitioned hybrid execution.

The included sum, maximum, and count-even plugins provide native C callbacks
and separate WRAM/MRAM DPU kernels. Tasklet counts from 1 through 24 are
supported.

Build and test it from the repository root:

```bash
make -C upimulator/engine/c
UPIMULATOR_ROOT=~/pim/upimulator/golang/uPIMulator \
  make -C upimulator/engine/c test
```

See [`c/README.md`](c/README.md) for the architecture, plugin API, installation,
run commands, presentation demo, and current limitations.
