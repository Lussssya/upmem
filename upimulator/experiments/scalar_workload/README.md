# Generic scalar-workload simulator adapter

This benchmark is infrastructure for runtime-loaded C workload plugins. The
runner copies the plugin-selected, self-contained DPU `task.c` into the generic
benchmark slot, compiles it for the requested tasklet count, and runs it.

The Go adapter knows only the shared ABI symbols (`array_length`, optional
WRAM `input_values`, and `output_value`). It serializes arbitrary uint32 input
values and uses the expected partition result supplied by the loaded plugin for
uPIMulator's internal validation. The engine still obtains its DPU result from
the separately captured actual output bytes.
