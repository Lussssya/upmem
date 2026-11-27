# CPU metadata filter to DPU exact search

This correctness experiment filters SIFT-small base vectors by synthetic upload
date on the native CPU, transfers the eligible vectors to one simulated UPMEM
DPU, and performs exact squared-L2 nearest-neighbor search in a real DPU C
kernel.

The backend is the **UPMEM SDK 2023.1.0 functional simulator** selected with
`backend=simulator`. It is not the cycle-level Go uPIMulator and it is not
physical DPU hardware.

## Data and metadata

The experiment reuses the existing `ann/faiss_cpp` TEXMEX loader without
changing it. Synthetic metadata is deterministic:

```text
base vector ID i -> upload date 2000-01-01 + i days
```

For 10,000 SIFT-small base vectors, dates run from 2000-01-01 through
2027-05-18. Filtering preserves each vector's original database ID.

## Execution order

1. The host loads base and query `.fvecs` files.
2. `metadata.cpp` generates dates and selects IDs satisfying `upload_date >=`
   the requested date.
3. The host independently computes the exact CPU reference on those candidates.
4. The host allocates one DPU with `backend=simulator` and transfers the query,
   filtered vectors, 64-bit transport copies of their original IDs, and an
   8-byte configuration structure.
5. The DPU reads each 512-byte float vector from MRAM into WRAM, computes
   float32 squared L2, and maintains the best K results.
6. The host retrieves the 48-byte output and compares count, IDs, ranking, and
   distances against the independent CPU reference.

The DPU has no hardware floating-point unit. This SDK compiles float operations
to compiler-rt soft-float routines. The metric remains float32 squared L2; it
is not converted to integer arithmetic. CPU compilation disables fused
multiply-add contraction. Results use deterministic `(distance, original ID)`
tie ordering and are compared with
`abs_error <= 1e-3 + 1e-6 * abs(reference)`.

## Memory layout

- Query: 128 float32 values in MRAM, read once into a 512-byte WRAM buffer.
- Candidates: up to 10,000 x 128 float32 values in MRAM, read one vector at a
  time into a second 512-byte WRAM buffer.
- IDs: one aligned uint64 transport value per candidate in MRAM.
- Computation: one DPU and one tasklet.
- Results: up to five original uint32 IDs and float32 distances in WRAM.

Every MRAM DMA operation is aligned, 8-byte granular, and at most 512 bytes,
within the SDK's 8-to-2048-byte DMA requirement. The maximum candidate payload
is about 5.2 MB, below one DPU's 64 MiB MRAM.

## Build and run

```bash
cd /home/vboxuser/pim/upmem-tests/ann/metadata_dpu_search
make
make run ARGS="--min-date 2027-05-01 --query-id 0 --k 5"
```

Or initialize the SDK environment and run the binary directly:

```bash
. /home/vboxuser/pim/upmem-sdk/upmem_env.sh simulator
./build/metadata_dpu_search \
    --dataset-dir ../datasets/siftsmall \
    --min-date 2026-01-01 --query-id 17 --k 5
```

Run all correctness cases:

```bash
make test
```

The test suite covers restrictive and less restrictive dates, no candidates,
fewer than K candidates, and multiple query IDs.

