# Generic C CPU–DPU execution engine

The engine distributes a contiguous `uint32_t` array between the native CPU
and one simulated DPU. The operation itself is supplied by a runtime-loaded C
plugin. The engine has no operation switch and does not need to be rebuilt when
a compatible plugin is added.

## Architecture

```text
                    runtime-loaded workload plugin (.so)
                    | CPU compute / combine / reference
                    | WRAM task.c / MRAM task.c
                    v
CLI -> plugin loader -> input loader -> contiguous partitioner
                              |                    |
                              | CPU partition      | DPU partition
                              v                    v
                    hybrid CPU worker       main-thread DPU backend
                              |                    |
                    native plugin callback  runner -> uPIMulator -> task.c
                              |                    |
                              `------ join --------'
                                      |
                                      v
                           combine + correctness check
```

The engine owns input parsing, plugin loading, CPU/DPU/hybrid mode selection,
contiguous partitioning, backend invocation, output decoding, and correctness
reporting. A plugin owns the mathematical operation, empty-partition identity,
partial-result combination, reference result, and both DPU kernels.

In hybrid mode, the engine starts the native CPU callback in a POSIX worker
thread and immediately launches the simulator from the main thread. It waits
for both backends before combining their independent partial results. The DPU
runner is started with `posix_spawn()`, avoiding child-side application code
after `fork()` in the multithreaded process. CPU-only and DPU-only modes retain
their direct sequential backend calls.

This overlap is a host execution property, not a performance claim. Reported
`logic_cycle` remains simulated DPU time only; it does not include native CPU
time, simulator wall-clock time, or an end-to-end heterogeneous speedup.

The CPU worker is always joined before plugin unload. A join failure has no safe
recovery path because the worker may still reference engine stack storage and
execute plugin code. In that exceptional case the engine prints a fatal error
and terminates with `_Exit()` so it cannot unwind the stack or call `dlclose()`.

The public ABI is [`include/workload_plugin.h`](include/workload_plugin.h).
It supports this deliberately limited family:

```text
uint32_t input array -> uint32_t scalar result
```

A plugin provides:

- ABI version, workload ID, display name, description, and element limit;
- the identity result for an empty partition;
- input validation and native CPU partition computation;
- partial-result combination and full reference computation;
- the generic runner name and WRAM/MRAM kernel paths.

Every callback returns `0` on success and `-1` with an error message on
failure. Results, identities, partial results, and combined results are
`uint32_t`. Workloads such as sum must explicitly reject arithmetic overflow.

## DPU kernel contract

Each plugin supplies self-contained WRAM and MRAM `task.c` files compiled for
the UPMEM ISA. Both expose:

```c
__host uint32_t array_length;
__host uint32_t output_value;
```

The WRAM variant additionally exposes:

```c
__host uint32_t input_values[256];
```

The generic Go adapter serializes values and does not know the operation. The
engine computes the DPU partition's validation value through the plugin and
passes it to uPIMulator because the simulator requires expected output for its
internal comparison. The engine's DPU partial result is still decoded from the
separately captured actual output bytes.

The C backend starts the plugin-selected generic runner. That runner copies the
selected kernel into the generic benchmark slot, compiles it with the requested
`NR_TASKLETS`, starts the Go uPIMulator, and leaves `actual_output.bin` and
`log.txt`. The backend removes stale copies first, strictly decodes the actual
four-byte result, and reads `Logic[0_0_0]_logic_cycle` from the fresh log.

In the included kernels, WRAM tasklet `t` processes indices
`t, t + NR_TASKLETS, ...`. MRAM tasklets process disjoint two-element
(eight-byte) blocks with `mram_read()`. Every tasklet writes one partial result,
all wait at a barrier, and tasklet 0 combines the partials. Non-divisible arrays
and tasklets that receive no work are handled naturally.

## Build and install

```bash
./upimulator/install.sh ~/pim/upimulator/golang/uPIMulator
make -C upimulator/engine/c
```

The installer registers the generic simulator adapter once. Adding another
plugin later does not require another Go adapter or registry change.

## Run

```bash
upimulator/engine/c/engine \
  --workload upimulator/engine/c/workloads/sum/plugin.so \
  --mode hybrid --cpu-share 0.4 \
  --dpu-memory mram --tasklets 8 \
  --input upimulator/engine/examples/mixed_values.txt
```

```bash
upimulator/engine/c/engine \
  --workload upimulator/engine/c/workloads/max/plugin.so \
  --mode dpu --dpu-memory wram --tasklets 4 \
  --input upimulator/engine/examples/mixed_values.txt
```

Input files contain 1–256 decimal `uint32_t` values separated by commas or
whitespace. `#` starts a comment. A plugin may impose additional constraints.

Inspect a plugin without running a workload:

```bash
upimulator/engine/c/engine \
  --workload upimulator/engine/c/workloads/count_even/plugin.so \
  --describe-workload
```

## Presentation demo

The demo uses `[3, 4, 7, 8, 10]`, whose independently known results are sum
`32`, maximum `10`, and even count `3`. It runs six CPU-only, DPU-only, and
hybrid configurations across all three plugins. It stops on any command error,
incorrect result, or missing PASS status.

```bash
make -C upimulator/engine/c
UPIMULATOR_ROOT=~/pim/upimulator/golang/uPIMulator \
  ./upimulator/engine/c/demo.sh
```

## Add a workload

See [`workloads/README.md`](workloads/README.md). In short:

1. create `workloads/<name>/plugin.c` implementing the public ABI;
2. add `dpu/wram/task.c` and `dpu/mram/task.c` using the shared symbol contract;
3. run `make -C upimulator/engine/c` (the wildcard build discovers it);
4. pass the new `plugin.so` to `--workload`.

No core source, partitioner, DPU backend, Go adapter, or simulator registry
change is needed. `workloads/count_even/` is a proof alongside maximum: it was
added without operation-specific changes to those components.

## Test

```bash
UPIMULATOR_ROOT=~/pim/upimulator/golang/uPIMulator \
  make -C upimulator/engine/c test
```

The fake runners are used only for malformed-output, stale-file, bad-cycle,
incorrect-result, and explicit CPU/DPU-overlap checks. Other DPU cases execute
the real Go simulator.

Normal C-engine build and execution do not invoke Python. The one-time
`upimulator/install.sh` compatibility step and uPIMulator's native build
helpers still use Python.

Output directories and the generic benchmark source slot are shared, so
concurrent engine invocations are not supported.
