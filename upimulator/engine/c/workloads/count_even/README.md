# Count-even workload

This independently loadable plugin counts even values in a `uint32_t` array.
Its empty-partition identity is zero, and partial counts are combined by
addition.

- WRAM: tasklet `t` processes indices `t, t + NR_TASKLETS, ...`.
- MRAM: tasklets process disjoint two-value, eight-byte DMA blocks.
- After a barrier, tasklet 0 adds every tasklet's partial count.

The generic engine, partitioner, DPU backend, Go adapter, and plugin ABI do not
contain count-even-specific code.
