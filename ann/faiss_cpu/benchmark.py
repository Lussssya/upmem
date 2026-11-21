#!/usr/bin/env python3
"""Repeated CPU benchmarks and IVF workload analysis using ivf_flat helpers."""

import argparse
import csv
from dataclasses import dataclass
import json
import os
from pathlib import Path
import platform
import sys

import faiss
import numpy as np
from threadpoolctl import threadpool_info, threadpool_limits

import ivf_flat as baseline


DEFAULT_CONFIG = Path(__file__).resolve().parent / "configs" / "large.json"
SCAN_LABEL = (
    "Estimated scan workload: total database vectors in selected lists; "
    "not an instrumented FAISS distance-computation count."
)


def cluster_statistics(sizes, expected_total):
    """Population statistics include every inverted list, including empty ones."""
    sizes = np.asarray(sizes)
    if (sizes.ndim != 1 or not sizes.size or sizes.dtype.kind not in "iu"
            or np.any(sizes < 0)):
        raise ValueError("cluster sizes must be a nonempty array of nonnegative integers")
    if int(sizes.sum()) != expected_total or expected_total <= 0:
        raise ValueError("cluster sizes must sum to the positive indexed vector count")
    mean = float(sizes.mean())
    return {
        "nlist": len(sizes), "total_vectors": int(sizes.sum()),
        "minimum": int(sizes.min()), "maximum": int(sizes.max()),
        "mean": mean, "std_population": float(sizes.std(ddof=0)),
        "empty_clusters": int(np.count_nonzero(sizes == 0)),
        "largest_to_mean_ratio": float(sizes.max() / mean),
    }


@dataclass
class QueryWorkload:
    selected_clusters: np.ndarray
    query_counts: np.ndarray
    estimated_scan_vectors: np.ndarray
    summary: dict


def summarize_workload(selected, sizes):
    """Count each selected list once per query; use list sizes as a work estimate."""
    sizes = np.asarray(sizes)
    cluster_statistics(sizes, int(sizes.sum()))
    selected = np.asarray(selected)
    if (selected.ndim != 2 or not all(selected.shape)
            or selected.dtype.kind not in "iu" or np.any(selected < 0)
            or np.any(selected >= len(sizes))):
        raise ValueError("selected clusters must be a nonempty (nq, nprobe) ID matrix")
    ordered = np.sort(selected, axis=1)
    if np.any(np.diff(ordered, axis=1) == 0):
        raise ValueError("a query must not select a cluster more than once")
    nq, nprobe = selected.shape
    counts = np.bincount(selected.ravel(), minlength=len(sizes))
    scan = sizes[selected].sum(axis=1, dtype=np.int64)
    if int(counts.sum()) != nq * nprobe or int(scan.sum()) != int(counts @ sizes):
        raise RuntimeError("inconsistent query workload totals")
    mean_traffic = float(counts.mean())
    summary = {
        "nprobe": nprobe, "queries": nq, "total_cluster_selections": int(counts.sum()),
        "traffic_min": int(counts.min()), "traffic_max": int(counts.max()),
        "traffic_mean": mean_traffic, "traffic_std_population": float(counts.std(ddof=0)),
        "traffic_cv": float(counts.std(ddof=0) / mean_traffic),
        "traffic_max_to_mean_ratio": float(counts.max() / mean_traffic),
        "unselected_clusters": int(np.count_nonzero(counts == 0)),
        "clusters_at_least_2x_mean_traffic": int(np.count_nonzero(counts >= 2 * mean_traffic)),
        "estimated_scan_vectors_total": int(scan.sum()),
        "estimated_scan_vectors_min_per_query": int(scan.min()),
        "estimated_scan_vectors_max_per_query": int(scan.max()),
        "estimated_scan_vectors_mean_per_query": float(scan.mean()),
        "estimated_scan_vectors_median_per_query": float(np.median(scan)),
        "estimated_scan_vectors_std_population_per_query": float(scan.std(ddof=0)),
        "estimated_scan_vectors_p95_per_query": float(np.percentile(scan, 95)),
        "estimated_scan_database_fraction_mean": float(scan.mean() / sizes.sum()),
    }
    return QueryWorkload(selected, counts, scan, summary)


def analyze_queries(index, queries, sizes, nprobe):
    """Ask the trained FAISS coarse quantizer which lists it would select."""
    if not index.is_trained or not 1 <= nprobe <= index.nlist:
        raise ValueError("query analysis requires a trained index and valid nprobe")
    cluster_statistics(sizes, index.ntotal)
    if len(sizes) != index.nlist:
        raise ValueError("cluster sizes must contain one entry per inverted list")
    queries = baseline.validate_vectors(queries, "queries", index.d)
    _, selected = index.quantizer.search(queries, nprobe)
    return summarize_workload(selected, sizes)


def timing_statistics(seconds, nq):
    seconds = np.asarray(seconds, dtype=np.float64)
    if (seconds.ndim != 1 or len(seconds) < 5 or nq < 1
            or not np.isfinite(seconds).all() or np.any(seconds <= 0)):
        raise ValueError("timing statistics require at least 5 positive finite runs and nq > 0")
    latency = seconds * 1000 / nq
    q25, median, q75 = np.percentile(latency, [25, 50, 75])
    return {
        "measured_repetitions": len(seconds), "mean_search_seconds": float(seconds.mean()),
        "median_search_seconds": float(np.median(seconds)),
        "mean_latency_ms_per_query": float(latency.mean()),
        "median_latency_ms_per_query": float(median),
        "latency_q25_ms": float(q25), "latency_q75_ms": float(q75),
        "latency_iqr_ms": float(q75 - q25),
        "latency_std_sample_ms": float(latency.std(ddof=1)),
        "latency_min_ms": float(latency.min()), "latency_max_ms": float(latency.max()),
        "qps_from_median": float(nq / np.median(seconds)),
        "qps_from_mean": float(nq / seconds.mean()),
    }


def benchmark_searches(index, exact_index, queries, k, nprobes, repeats, warmup,
                       seed, progress=print):
    """Warm each case, then shuffle case order in each measured round."""
    if repeats < 5 or warmup < 1:
        raise ValueError("benchmark requires --repeats >= 5 and --warmup >= 1")
    cases = [0, *nprobes]  # Zero denotes the separate exact Flat index.
    queries = baseline.validate_vectors(queries, "queries", index.d)

    def active_index(case):
        if case:
            index.nprobe = int(case)
            return index
        return exact_index

    for case in cases:
        progress(f"Warmup: {'Flat' if not case else f'IVF nprobe={case}'}", flush=True)
        active = active_index(case)
        for _ in range(warmup):
            active.search(queries, k)
    timings = {case: [] for case in cases}
    results = {}
    raw_rows = []
    rng = np.random.default_rng(seed)
    for repeat in range(repeats):
        for position, case in enumerate(rng.permutation(cases), start=1):
            case = int(case)
            result = baseline.search_index(active_index(case), queries, k, repeats=1, warmup=0)
            seconds = result.seconds_per_run[0]
            timings[case].append(seconds)
            results[case] = result
            raw_rows.append({
                "round": repeat + 1, "position": position,
                "index": "IVF" if case else "Flat", "nprobe": case if case else "",
                "search_seconds": seconds, "latency_ms_per_query": seconds * 1000 / len(queries),
                "qps": len(queries) / seconds,
            })
        progress(f"Measured round {repeat + 1}/{repeats} complete", flush=True)
    exact_median = float(np.median(timings[0]))
    exact_mean = float(np.mean(timings[0]))
    rows = []
    for case in cases:
        metrics = timing_statistics(timings[case], len(queries))
        rows.append({
            "index": "IVF" if case else "Flat", "nprobe": case if case else "",
            "k": k, "nq": len(queries),
            "recall_at_k": baseline.recall_at_k(results[case].ids, results[0].ids),
            **metrics,
            "speedup_vs_flat_median": exact_median / metrics["median_search_seconds"],
            "speedup_vs_flat_mean": exact_mean / metrics["mean_search_seconds"],
            "missing_neighbors": int(np.count_nonzero(results[case].ids < 0)),
        })
    return rows, raw_rows, results


def write_csv(path, fieldnames, rows):
    with Path(path).open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_records(path, rows):
    write_csv(path, list(rows[0]), rows)


def export_statistics(directory, sizes, clusters, workloads, benchmarks, raw_rows):
    """Stream detailed selection rows rather than building a large Python table."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    write_records(directory / "benchmark_results.csv", benchmarks)
    write_records(directory / "search_repetitions.csv", raw_rows)
    write_records(directory / "cluster_statistics.csv", [clusters])
    write_csv(directory / "cluster_sizes.csv", ["cluster_id", "size", "fraction", "size_to_mean_ratio"], (
        {"cluster_id": cluster, "size": int(size), "fraction": float(size / sizes.sum()),
         "size_to_mean_ratio": float(size / sizes.mean())}
        for cluster, size in enumerate(sizes)
    ))
    write_records(directory / "workload_summary.csv", [work.summary for work in workloads.values()])
    write_csv(directory / "query_workload.csv", [
        "nprobe", "query_id", "estimated_scan_vectors", "estimated_scan_database_fraction"
    ], (
        {"nprobe": nprobe, "query_id": query, "estimated_scan_vectors": int(scan),
         "estimated_scan_database_fraction": float(scan / sizes.sum())}
        for nprobe, work in workloads.items() for query, scan in enumerate(work.estimated_scan_vectors)
    ))
    write_csv(directory / "query_selected_lists.csv", [
        "nprobe", "query_id", "centroid_rank", "cluster_id", "list_size"
    ], (
        {"nprobe": nprobe, "query_id": query, "centroid_rank": rank,
         "cluster_id": int(cluster), "list_size": int(sizes[cluster])}
        for nprobe, work in workloads.items()
        for query, selected in enumerate(work.selected_clusters)
        for rank, cluster in enumerate(selected, start=1)
    ))
    write_csv(directory / "cluster_query_traffic.csv", [
        "nprobe", "cluster_id", "size", "query_count", "query_fraction",
        "selection_share", "traffic_to_mean_ratio", "estimated_scan_vectors_total"
    ], (
        {"nprobe": nprobe, "cluster_id": cluster, "size": int(sizes[cluster]),
         "query_count": int(count), "query_fraction": float(count / len(work.selected_clusters)),
         "selection_share": float(count / work.query_counts.sum()),
         "traffic_to_mean_ratio": float(count / work.query_counts.mean()),
         "estimated_scan_vectors_total": int(count * sizes[cluster])}
        for nprobe, work in workloads.items() for cluster, count in enumerate(work.query_counts)
    ))


def make_plots(directory, sizes, benchmarks, k):
    """Headless, standalone PNG figures; plotting is outside all search timers."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    directory = Path(directory)
    fig, axis = plt.subplots(figsize=(11, 4), constrained_layout=True)
    axis.bar(np.arange(len(sizes)), sizes, color="#3274a1", width=0.9)
    axis.axhline(sizes.mean(), color="#b13d36", linestyle="--", label=f"Mean = {sizes.mean():.2f}")
    axis.set(xlabel="IVF cluster ID", ylabel="Database vectors", title="Inverted-list sizes (empty lists included)")
    axis.legend()
    fig.savefig(directory / "cluster_sizes.png", dpi=160)
    plt.close(fig)

    ivf = sorted((row for row in benchmarks if row["index"] == "IVF"), key=lambda row: row["nprobe"])
    flat = next(row for row in benchmarks if row["index"] == "Flat")
    probes = [row["nprobe"] for row in ivf]
    recalls = [row["recall_at_k"] for row in ivf]
    medians = np.array([row["median_latency_ms_per_query"] for row in ivf])
    errors = np.array([
        medians - np.array([row["latency_q25_ms"] for row in ivf]),
        np.array([row["latency_q75_ms"] for row in ivf]) - medians,
    ])
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), constrained_layout=True)
    axes[0].plot(probes, recalls, "o-", color="#3274a1")
    axes[0].set_xscale("log", base=2)
    axes[0].set_xticks(probes, labels=[str(probe) for probe in probes])
    axes[0].set(xlabel="nprobe", ylabel=f"Recall@{k}", ylim=(-0.03, 1.06), title="Lists probed and recall")
    axes[1].errorbar(recalls, medians, yerr=errors, fmt="o-", capsize=4,
                     color="#3274a1", label="IVF median; bars = IQR")
    for recall, latency, probe in zip(recalls, medians, probes):
        axes[1].annotate(str(probe), (recall, latency), xytext=(5, 5), textcoords="offset points", fontsize=8)
    axes[1].axhline(flat["median_latency_ms_per_query"], color="#b13d36", linestyle="--", label="Exact Flat median")
    axes[1].set(xlabel=f"Recall@{k}", ylabel="Median amortized latency (ms/query)",
                xlim=(-0.03, 1.12), title="Recall / latency trade-off (labels = nprobe)")
    axes[1].legend(fontsize=8)
    for axis in axes:
        axis.grid(alpha=0.2)
    fig.savefig(directory / "recall_latency.png", dpi=160)
    plt.close(fig)


def print_tables(clusters, benchmarks, workloads):
    print("\nCluster distribution (population std includes empty lists)")
    print(f"{'vectors':>10} {'lists':>7} {'min':>7} {'max':>7} {'mean':>10} {'std':>10} {'empty':>7} {'max/mean':>10}")
    print(f"{clusters['total_vectors']:10d} {clusters['nlist']:7d} {clusters['minimum']:7d} "
          f"{clusters['maximum']:7d} {clusters['mean']:10.2f} {clusters['std_population']:10.2f} "
          f"{clusters['empty_clusters']:7d} {clusters['largest_to_mean_ratio']:10.3f}")
    k = benchmarks[0]["k"]
    print(f"\nSearch benchmark: Recall@{k}; amortized batch latency per query")
    print(f"{'index':<8} {'nprobe':>7} {'recall':>8} {'mean ms':>10} {'median ms':>10} {'IQR ms':>10} {'std ms':>10} {'QPS(med)':>11} {'speedup':>9}")
    for row in benchmarks:
        print(f"{row['index']:<8} {str(row['nprobe']):>7} {row['recall_at_k']:8.4f} "
              f"{row['mean_latency_ms_per_query']:10.5f} {row['median_latency_ms_per_query']:10.5f} "
              f"{row['latency_iqr_ms']:10.5f} {row['latency_std_sample_ms']:10.5f} "
              f"{row['qps_from_median']:11.1f} {row['speedup_vs_flat_median']:8.3f}x")
    print("\n" + SCAN_LABEL)
    print(f"{'nprobe':>7} {'mean vectors/q':>15} {'p95 vectors/q':>15} {'traffic CV':>12} {'max/mean':>10} {'unselected':>11} {'top cluster:queries':>22}")
    for nprobe, work in workloads.items():
        summary = work.summary
        top = int(np.argmax(work.query_counts))
        print(f"{nprobe:7d} {summary['estimated_scan_vectors_mean_per_query']:15.1f} "
              f"{summary['estimated_scan_vectors_p95_per_query']:15.1f} "
              f"{summary['traffic_cv']:12.3f} {summary['traffic_max_to_mean_ratio']:10.3f} "
              f"{summary['unselected_clusters']:11d} {f'{top}:{work.query_counts[top]}':>22}")
    print("Traffic ratios/CV describe this query sample; they are not significance tests.")


def make_parser():
    parser = baseline.make_parser()
    parser.description = __doc__
    parser.epilog = "Defaults come from configs/large.json; custom JSON and explicit CLI options override them."
    parser.set_defaults(examples=0)
    help_text = {
        "dim": "vector dimension (defaults from JSON; loaded base must match)",
        "nb": "generated database vector count (defaults from JSON)",
        "nq": "generated query vector count (defaults from JSON)",
        "output_dir": "directory for benchmark CSVs, arrays and figures",
        "examples": "number of example query results to print (default: 0)",
    }
    for action in parser._actions:
        if action.dest in help_text:
            action.help = help_text[action.dest]
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                        help="JSON defaults; explicit CLI options override them")
    parser.add_argument("--no-plots", action="store_true", help="skip PNG generation")
    return parser


def parse_args(argv=None):
    """Apply typed JSON defaults through the existing CLI's validators."""
    parser = make_parser()
    preliminary = argparse.ArgumentParser(add_help=False)
    preliminary.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    config_path = preliminary.parse_known_args(argv)[0].config
    try:
        config = json.loads(DEFAULT_CONFIG.read_text())
        custom = json.loads(config_path.read_text())
        if not isinstance(custom, dict):
            raise ValueError("configuration must be a JSON object")
        config.update(custom)
        actions = {action.dest: action for action in parser._actions
                   if action.dest not in {"help", "config"}}
        defaults = {}
        for key, value in config.items():
            if key not in actions:
                raise ValueError(f"unknown configuration key: {key}")
            action = actions[key]
            if value is None and action.default is None:
                defaults[key] = None
            elif isinstance(action.default, bool):
                if not isinstance(value, bool):
                    raise ValueError(f"{key} must be boolean")
                defaults[key] = value
            elif action.nargs == "+":
                if not isinstance(value, list) or not value:
                    raise ValueError(f"{key} must be a nonempty list")
                defaults[key] = [action.type(str(item)) for item in value]
            else:
                defaults[key] = action.type(str(value))
        parser.set_defaults(**defaults)
    except (OSError, ValueError, TypeError, argparse.ArgumentTypeError) as error:
        parser.error(f"configuration {config_path}: {error}")
    args = parser.parse_args(argv)
    if args.repeats < 5 or args.warmup < 1:
        parser.error("benchmark requires --repeats >= 5 and --warmup >= 1")
    if args.output_dir is None:
        parser.error("benchmark requires --output-dir (or output_dir in JSON)")
    if args.train_file is not None and args.train_size is not None:
        parser.error("--train-file and --train-size are mutually exclusive")
    return args


def run(args):
    if not 0 <= args.seed <= 2**31 - 1:
        raise ValueError("--seed must lie in [0, 2147483647]")
    if not args.no_plots:
        # Fail before a long experiment if the plotting dependency is missing.
        import matplotlib  # noqa: F401
    with threadpool_limits(limits=args.threads):
        faiss.omp_set_num_threads(args.threads)
        dataset = baseline.prepare_dataset(args)
        nprobes = list(dict.fromkeys(args.nprobe))
        baseline.validate_configuration(dataset, args.nlist, nprobes, args.k)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        print(f"Dataset: nb={len(dataset.base)}, dim={dataset.base.shape[1]}, nq={len(dataset.queries)}, "
              f"training={len(dataset.training)}, nlist={args.nlist}, k={args.k}, threads={args.threads}", flush=True)
        index, build_times = baseline.build_ivf(dataset.base, dataset.training, args.nlist, args.seed)
        clusters = baseline.inspect_clusters(index)
        cluster_summary = cluster_statistics(clusters.sizes, index.ntotal)
        print(f"IVF trained={index.is_trained}, ntotal={index.ntotal}; build times: {build_times}", flush=True)
        exact_index = baseline.build_exact(dataset.base)
        benchmarks, raw_rows, results = benchmark_searches(
            index, exact_index, dataset.queries, args.k, nprobes,
            args.repeats, args.warmup, args.seed,
        )
        # Analysis is intentionally after all measured searches.
        workloads = {probe: analyze_queries(index, dataset.queries, clusters.sizes, probe)
                     for probe in nprobes}
        for row in benchmarks:
            row.update({"threads": args.threads, "nb": len(dataset.base),
                        "dim": dataset.base.shape[1], "nlist": args.nlist,
                        "seed": args.seed, "warmup_runs": args.warmup})
        report = {
            "configuration": {key: str(value) if isinstance(value, Path) else value
                              for key, value in vars(args).items()},
            "dataset": {"nb": len(dataset.base), "nq": len(dataset.queries),
                        "dimension": dataset.base.shape[1], "training_count": len(dataset.training)},
            "environment": {
                "python": platform.python_version(), "faiss": faiss.__version__,
                "numpy": np.__version__, "platform": platform.platform(),
                "machine": platform.machine(), "processor": platform.processor(),
                "logical_cpu_count": os.cpu_count(), "faiss_threads": faiss.omp_get_max_threads(),
                "faiss_compile_options": faiss.get_compile_options(), "thread_pools": threadpool_info(),
            },
            "index": {"is_trained": bool(index.is_trained), "ntotal": index.ntotal,
                      "clustering_seed": int(index.cp.seed), "clustering_iterations": int(index.cp.niter),
                      "max_points_per_centroid": int(index.cp.max_points_per_centroid),
                      "max_codes": int(index.max_codes)},
            "build": build_times, "clusters": cluster_summary, "benchmarks": benchmarks,
            "workload_summary": [work.summary for work in workloads.values()],
            "measurement": {
                "clock": "time.perf_counter; native CPU wall-clock seconds inside this environment",
                "latency": "Each timed full-query batch divided by nq; not isolated-query or tail latency",
                "schedule": "Each measured round contains Flat and all nprobes in seed-shuffled order",
                "speedup": "Flat median batch seconds / IVF median batch seconds; >1 means IVF faster",
                "variability": "IQR: 75th minus 25th percentile; latency std uses ddof=1 across repetitions",
                "scan_workload": SCAN_LABEL,
                "traffic": "Descriptive sample statistics; >=2x mean is a diagnostic threshold, not a significance test",
                "excluded": "All index construction, coarse-quantizer workload analysis, recall, export and plotting",
            },
        }
        # Reuse the baseline's array/result exporter without changing its behavior.
        baseline.export_experiment(args.output_dir, dataset, clusters, results[0],
                                   {probe: results[probe] for probe in nprobes}, report)
        export_statistics(args.output_dir, clusters.sizes, cluster_summary, workloads, benchmarks, raw_rows)
        print_tables(cluster_summary, benchmarks, workloads)
        for query in range(min(args.examples, len(dataset.queries))):
            print(f"Query {query}:")
            for case, result in sorted(results.items()):
                label = "Flat" if not case else f"IVF nprobe={case}"
                print(f"  {label} IDs: {result.ids[query].tolist()}")
                print(f"    squared L2: {result.distances[query].tolist()}")
        if not args.no_plots:
            make_plots(args.output_dir, clusters.sizes, benchmarks, args.k)
        print(f"\nExported benchmark, workload CSVs and figures to {args.output_dir}", flush=True)
    return report


def main(argv=None):
    args = parse_args(argv)
    try:
        run(args)
    except (ValueError, OSError, RuntimeError, ImportError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
