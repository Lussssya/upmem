# Distributed-sum MRAM backend

This is the MRAM variant used by the C distribution engine. The Go adapter
places the host-supplied values in the simulated MRAM heap. DPU tasklets claim
two-element (eight-byte) blocks cyclically, explicitly transfer each block to
a private WRAM cache with `mram_read()`, and reduce their partial sums after a
barrier. Eight bytes is the minimum aligned DMA transfer and lets small inputs
be divided among more tasklets than a larger block would allow.

The host-to-MRAM setup transfer and the kernel's internal MRAM-to-WRAM DMA are
separate operations. The experiment does not claim the values originated in
MRAM before the host supplied them.

Run it through the shared runner:

```bash
./run_distributed_sum.sh --memory mram --tasklets 4 '5,9,2,100,7'
```
