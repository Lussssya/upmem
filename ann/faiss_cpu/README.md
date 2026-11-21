# FAISS IVF-Flat CPU baseline

This standalone Python CLI builds, inspects, and evaluates an L2 IVF-Flat index
using the official FAISS library and NumPy. FAISS performs clustering, database
insertion, and nearest-neighbor search. The code does not implement a separate
IVF algorithm. It does not depend on the UPMEM SDK, uPIMulator, or the scalar
CPU–DPU engine elsewhere in this repository.

For repeated timing, the 100,000-vector / 128-dimensional configuration, cluster
distribution and query workload analysis, CSV tables, and plots, see
[BENCHMARKS.md](BENCHMARKS.md). The separate `benchmark.py` driver reuses this
application's functions; the `ivf_flat.py` CLI and its original tests are preserved.

## How IVF-Flat works

FAISS learns `nlist` coarse centroids from training vectors using clustering.
Each database vector is stored in the inverted list belonging to its nearest
centroid. At query time, FAISS finds the `nprobe` nearest centroids, scans those
lists, and returns the best `k` vector IDs and **squared L2 distances**.
`IndexFlatL2` is the coarse quantizer, and the main index is
`IndexIVFFlat(quantizer, dimension, nlist, faiss.METRIC_L2)`.

“Flat” means that lists store full float32 vectors without product quantization.
Distances to scanned vectors are evaluated directly; approximation comes from
skipping lists. Larger `nlist` creates more partitions; larger `nprobe` scans more
partitions and generally increases recall and work. With `nprobe == nlist`, all
vectors are searched. A separate `IndexFlatL2` containing the same database
provides exact ground truth. Equal-distance ties may select or order IDs
differently, and floating-point distances may differ slightly between kernels.
The tests compare IDs on a deterministic dataset without ties and distances
with a tolerance.

See FAISS's [getting started guide](https://github.com/facebookresearch/faiss/wiki/Getting-started),
[IVF search tutorial](https://github.com/facebookresearch/faiss/wiki/Faster-search),
and [FAQ](https://github.com/facebookresearch/faiss/wiki/FAQ) for the underlying
APIs, clustering guidance, and numerical considerations.

## Install and run

Tested on Linux x86_64, Python 3.12.3, FAISS CPU 1.16.0,
NumPy 2.5.5, and one FAISS CPU thread. No GPU is required.

```sh
cd ann/faiss_cpu
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-lock.txt
python ivf_flat.py
python -m unittest discover -s tests -v
```

Run these commands from the repository root.
`requirements.txt` gives supported version ranges for other environments;
`requirements-lock.txt` pins the complete dependency set used in the VM checks.
On Debian/Ubuntu, a missing `venv` module can be installed with
`sudo apt install python3-venv`. Use a Python version and platform with available
FAISS CPU and NumPy wheels.

The default run uses 10,000 base vectors, 100 separate queries, dimension 64,
seed 42, 64 lists, `nprobe` values 1/4/16/64, `k=10`, and one FAISS thread.
It prints two example queries and does not write files unless requested.

Small reproducible experiment with exports:

```sh
python ivf_flat.py --nb 2000 --nq 20 --dim 16 \
  --nlist 8 --nprobe 1 2 4 8 --k 5 --seed 42 --threads 1 \
  --examples 1 --output-dir results/demo
```

A larger database with a reproducible training subset and more timing repeats:

```sh
python ivf_flat.py --nb 100000 --nq 1000 --dim 64 \
  --train-size 20000 --nlist 128 --nprobe 1 8 32 128 --k 10 \
  --seed 123 --threads 4 --warmup 2 --repeats 5 \
  --examples 0 --output-dir results/larger
```

Load exported arrays or your own `.npy` files:

```sh
python ivf_flat.py --base-file results/demo/base.npy \
  --query-file results/demo/queries.npy --train-file results/demo/training.npy \
  --nlist 8 --nprobe 1 4 8 --k 5 --threads 1 --output-dir results/reloaded
```

Files must contain finite, nonempty, two-dimensional float32 arrays of shape
`(number_of_vectors, dimension)`. A single query has shape `(1, dimension)`.
All dimensions must match; supplied `--dim` also checks a loaded base file.
The CLI rejects float64, integer, object, NaN, and infinity inputs instead of
silently converting them. Pickled NumPy data is disabled. To convert deliberately:

```python
import numpy as np
np.save("base.npy", original_vectors.astype(np.float32))
np.save("one_query.npy", original_queries[:1].astype(np.float32))
```

The base dimension is inferred when loading; `--nb` and `--nq` control only
generated arrays and do not truncate loaded arrays. Either file can be supplied
independently; missing arrays are generated. Synthetic data uses independent
NumPy random streams with uniform values in `[0, 1)` for database and queries.
Queries are not used for training. Training defaults to the full database;
`--train-size` samples database rows without replacement, while `--train-file`
loads a separate training matrix. These two options are mutually exclusive.
FAISS's own large-training-set subsampling may additionally apply, using its
seeded clustering defaults recorded in the summary.

Run `python ivf_flat.py --help` for all options. When changing `--nlist`, supply
appropriate `--nprobe` values too: every value must lie in `[1, nlist]`.
`k` must lie in `[1, database_size]`; training must contain at least `nlist`
vectors. Invalid settings exit with status 2 and an error message. A valid but
small training set below FAISS's recommended points per centroid emits a warning.
The seed must be an integer in `[0, 2147483647]`.

## Read the results

Recall@k is the mean over queries of
`|IVF_top_k_IDs intersect exact_top_k_IDs| / k`. A recall of 1.0 means every
ground-truth ID was recovered; ranking within those sets is not part of this
metric. If the selected lists contain fewer than `k` candidates, FAISS returns
ID `-1` with a sentinel distance for missing neighbors. They count as misses.
Ties at the kth distance can reduce ID-based recall even when the distances are
equally good.

Cluster counts come from the actual inverted lists, and exported assignments
come from the IDs stored in those lists. IDs are zero-based database row numbers.
Min/max/average include all `nlist` clusters, including empty lists. Large
differences between list sizes suggest potential work and storage imbalance.

Training and insertion have separate `time.perf_counter()` measurements around
FAISS's `train` and `add` calls. Each index receives `--warmup` untimed searches,
followed by `--repeats` timed searches of the full query batch (defaults: 1 and 3).
The table reports mean batch search seconds, mean batch time divided by query
count in milliseconds, and query count divided by mean batch seconds as QPS.
This latency is an amortized batch measurement, not isolated request latency.
Search timing includes the Python FAISS call and result allocation. It excludes
construction, data generation/loading, validation, cluster inspection, recall
calculation, printing, and file export. Exact-index insertion is not timed or
included in exact search time. Raw per-repeat timings are exported.

`--threads` calls `faiss.omp_set_num_threads()` before building or searching.
The seed controls both generated data and FAISS clustering. Record versions,
CPU/VM configuration, batch size, and thread settings for comparisons. Thread
counts and seeds improve reproducibility but cannot remove OS scheduling, cache,
BLAS-library, CPU-frequency, or VM effects; times are not deterministic. Small
synthetic uniform datasets are a correctness baseline, not representative ANN
workloads or guaranteed evidence that IVF outperforms Flat.

An actual run of the small command above produced this excerpt (times vary):

```text
Dataset: nb=2000, nq=20, dim=16, training=2000, threads=1
IVF trained=True, ntotal=2000, nlist=8
Training: 0.003178 s; insertion: 0.000296 s
Cluster sizes (including empty): {'count': 8, 'minimum': 227, 'maximum': 275, 'average': 250.0, 'empty': 0}
Vectors per cluster: [270, 249, 268, 228, 275, 234, 249, 227]
index                Recall@k      seconds     ms/query          QPS
Flat (exact)                -     0.000359     0.017956      55692.9
IVF nprobe=1         0.590000     0.000071     0.003558     281076.5
IVF nprobe=2         0.770000     0.000120     0.006007     166466.9
IVF nprobe=4         0.960000     0.000192     0.009604     104128.5
IVF nprobe=8         1.000000     0.000359     0.017927      55781.9
Query 0 exact IDs: [1707, 1077, 499, 1706, 768]
  exact squared L2: [0.8053207993507385, 0.8842839002609253, 0.9271671175956726, 0.9346874356269836, 1.0013506412506104]
```

## Export format

`--output-dir DIR` creates these files. Reusing a directory overwrites matching
filenames but leaves unrelated files, so use a new directory for each experiment.
All data and results are kept in memory; this is not an out-of-core benchmark.

| File | Contents |
| --- | --- |
| `base.npy` | Database float32 matrix `(nb, d)`; row index is vector ID |
| `queries.npy` | Query float32 matrix `(nq, d)` |
| `training.npy` | Supplied training float32 matrix, or selected database training rows |
| `centroids.npy` | Trained centroid float32 matrix `(nlist, d)` |
| `assignments.npy` | int64 array `(nb,)`: `assignments[vector_id] = cluster_id` |
| `cluster_sizes.npy` | int64 array `(nlist,)`, including zeros |
| `cluster_sizes.csv` | `cluster_id,size` table, including empty lists |
| `exact_results.npz` | Arrays `ids` and `squared_l2`, both shape `(nq, k)` |
| `ivf_nprobe_N.npz` | Same result arrays for each requested distinct nprobe |
| `summary.json` | Requested/effective configuration, library/platform/thread metadata, index state, construction times, cluster summary, recall, search metrics, raw repeat times, and missing-neighbor counts |

For example, `np.load("results/demo/ivf_nprobe_8.npz")["ids"]` loads neighbor
IDs. Arrays preserve database row IDs; exporting does not reorder base vectors.
The generated `.venv/`, `__pycache__/`, and `results/` are locally ignored by Git.
Generated NumPy arrays are ignored throughout this experiment; other output
directory names may need their own ignore rules. Compact recorded benchmark
results and figures are versioned separately in [`benchmarks/large/`](benchmarks/large/).

## Tests and future UPMEM use

The original 14 tests cover index construction,
centroids, stored assignments, empty clusters, result shapes, known neighbors
and squared distances, missing candidates, Recall@k, reproducible data and
clustering, invalid inputs/configurations, timing metrics, CLI error messages,
and loading/export round trips. It verifies that probing every list matches
exact Flat search, including single-query and `k == nb` cases. The deliberately
small-training test emits FAISS's expected clustering warning.

Run `python -m unittest discover -s tests -v` for the full suite. Installing
`benchmark-requirements-lock.txt` enables 12 additional benchmark/statistics tests
(26 total). With only the core dependencies, the benchmark test module is skipped
if its optional dependencies are unavailable, and the original tests still run.

Exported list sizes and assignments can support later studies of vector
partitioning, list storage, load imbalance, and CPU selection of candidate lists.
Centroids and exact results provide reference data for validating future ANN
implementations. Any later CPU–DPU design needs float vector inputs and top-k
outputs, beyond the existing engine's scalar uint32_t interface. This baseline
does not change that engine, add DPU kernels, implement IVF-PQ or HNSW, or measure
transfers or DPU execution.

**These measurements are native CPU wall-clock times inside the VM. uPIMulator
reports simulated DPU cycle counts. They are different quantities and must not
be treated as directly comparable measurements or used to infer CPU–DPU speedup.**
Such a comparison would require a separately validated hardware/timing model,
equivalent workloads, and clearly defined transfer and execution boundaries.
