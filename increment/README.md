# UPMEM functional-simulator round trip

This minimal C project explicitly allocates one DPU with
`backend=simulator`. The host sends `41`; the DPU returns `42` and writes a
message to its log. The host verifies the result and exits nonzero on failure.

```sh
make
make run
```
