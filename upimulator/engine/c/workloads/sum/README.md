# Array-sum plugin

Identity is zero. CPU partitions and the reference computation use checked
`uint64_t` accumulation and reject results above `UINT32_MAX`. Partial results
are combined with checked addition. WRAM and MRAM kernels perform the same
tasklet reductions as the original distributed-sum experiment.
