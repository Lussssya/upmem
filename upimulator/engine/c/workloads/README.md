# Writing a workload plugin

Plugins implement the versioned ABI in `../include/workload_plugin.h`. They are
ordinary native shared libraries; DPU kernels remain separate C sources that
the generic runner compiles for the UPMEM ISA.

Create this layout:

```text
workloads/example/
├── plugin.c
└── dpu/
    ├── wram/task.c
    └── mram/task.c
```

`plugin.c` exports exactly this entrypoint:

```c
const WorkloadPluginV1 *upmem_workload_plugin_v1(void);
```

Initialize every `WorkloadPluginV1` field. Use a filesystem-safe, unique
`workload_id`. Kernel paths are relative to `plugin.so`. For the standard
adapter, use `run_scalar_workload.sh` as `dpu_runner`.

Callbacks must follow these rules:

- `validate_input`: check operation-specific constraints;
- `compute_partition`: compute one native CPU partition and accept count zero;
- `combine`: combine two scalar partition results;
- `reference`: compute the full-array correctness result;
- all returned results must fit `uint32_t`;
- empty-partition behavior must match `identity`.

The WRAM and MRAM kernels must expose `array_length` and `output_value`; WRAM
also exposes `input_values[256]`. They are responsible for tasklet-safe work
distribution and reduction.

Build all discovered plugins:

```bash
make -C upimulator/engine/c
```

Or build one independently, without rebuilding the engine:

```bash
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -fPIC -shared -Iupimulator/engine/c/include \
  upimulator/engine/c/workloads/example/plugin.c \
  -o upimulator/engine/c/workloads/example/plugin.so
```

The maximum and count-even plugins are concrete proofs: each operation lives
entirely under its own directory and uses the unchanged generic engine,
partitioner, DPU backend, Go adapter, and ABI.
