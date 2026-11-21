# Recorded IVF-Flat CPU benchmark

Snapshot from 2026-11-20 using [`../../configs/large.json`](../../configs/large.json):
100,000 vectors, 128 dimensions, 1,000 queries, 128 lists, k=10, seed 42,
one CPU thread, two warmups, and five measured repetitions per case.

See [`../../BENCHMARKS.md`](../../BENCHMARKS.md) for the environment, measurement
definitions, results, and limitations. Latencies are amortized full-batch CPU
wall-clock measurements inside a VM.

- `benchmark_results.csv`: recall, timing statistics, QPS, and speedup.
- `search_repetitions.csv`: all 40 measured searches and their execution order.
- `cluster_statistics.csv`, `cluster_sizes.csv`: inverted-list distribution.
- `workload_summary.csv`, `cluster_query_traffic.csv`: estimated scan workload
  and cluster selection traffic.
- `summary.json`: settings, library versions, build times, and aggregate results.
- `cluster_sizes.png`, `recall_latency.png`: distribution and recall/latency plots.

Table values and plots are unchanged from the completed run; CSV line endings
are normalized to LF. The summary omits absolute shared-library paths. Generated
NumPy arrays, detailed per-query exports, virtual environments, and machine-local
files are not included.

Reproduce from `ann/faiss_cpu/` after installing the benchmark dependencies:

```sh
python benchmark.py --config configs/large.json
```

New output goes to ignored `results/large/`, preserving this recorded snapshot.
