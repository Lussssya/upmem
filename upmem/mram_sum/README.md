# Sum an MRAM array through a WRAM buffer

This example sums 256 integers with one simulated DPU and one tasklet while
making the DPU memory hierarchy explicit.

The host copies the full array into the DPU-side `mram_input` symbol. The DPU
does not calculate directly from that MRAM array. Instead, it processes eight
blocks of 32 integers:

```text
host array
  -> mram_input in DPU MRAM
  -> mram_read one 32-integer block
  -> 128-byte wram_buffer
  -> sum values from WRAM
  -> repeat for all eight blocks
  -> expose partial_sum to the host
```

Each `mram_read(&mram_input[block_start], wram_buffer, BLOCK_BYTES)` transfers
128 bytes. The source and destination are DMA-aligned, and the transfer size is
a multiple of 8 bytes as required by this SDK.

Build and run:

```sh
make clean
make
make run
```

This functional-simulator project demonstrates correct memory movement. It is
not a performance benchmark.
