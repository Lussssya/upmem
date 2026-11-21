#!/usr/bin/env python3
"""Inspectable CPU FAISS IVF-Flat baseline; IDs are database row numbers."""

import argparse
import csv
from dataclasses import dataclass
import json
from pathlib import Path
import platform
import sys
import time
import warnings

import faiss
import numpy as np


@dataclass
class Dataset:
    base: np.ndarray
    queries: np.ndarray
    training: np.ndarray


@dataclass
class Clusters:
    centroids: np.ndarray
    assignments: np.ndarray
    sizes: np.ndarray

    def summary(self):
        return {
            "count": len(self.sizes),
            "minimum": int(self.sizes.min()),
            "maximum": int(self.sizes.max()),
            "average": float(self.sizes.mean()),
            "empty": int(np.count_nonzero(self.sizes == 0)),
        }


@dataclass
class SearchResult:
    ids: np.ndarray
    distances: np.ndarray
    seconds_per_run: list[float]

    def metrics(self):
        seconds = float(np.mean(self.seconds_per_run))
        return {
            "search_seconds": seconds,
            "latency_ms_per_query": seconds * 1000 / len(self.ids),
            "queries_per_second": len(self.ids) / seconds,
            "seconds_per_run": self.seconds_per_run,
            "missing_neighbors": int(np.count_nonzero(self.ids < 0)),
        }


def positive_int(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return number


def nonnegative_int(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    return number


def validate_vectors(vectors, name, dimension=None):
    """Reject ambiguous inputs instead of silently casting or reshaping them."""
    if not isinstance(vectors, np.ndarray) or vectors.ndim != 2:
        raise ValueError(f"{name} must be a 2-D NumPy array (rows, dimension)")
    if not all(vectors.shape):
        raise ValueError(f"{name} must contain at least one vector and dimension")
    if vectors.dtype != np.float32:
        raise ValueError(f"{name} must have dtype float32, got {vectors.dtype}")
    if dimension is not None and vectors.shape[1] != dimension:
        raise ValueError(f"{name} dimension {vectors.shape[1]} != expected {dimension}")
    if not np.isfinite(vectors).all():
        raise ValueError(f"{name} contains NaN or infinity")
    return np.ascontiguousarray(vectors)


def load_vectors(path, name):
    if Path(path).suffix.lower() != ".npy":
        raise ValueError(f"{name} file must have a .npy extension")
    return validate_vectors(np.load(path, allow_pickle=False), name)


def prepare_dataset(args):
    """Generate independent uniform [0,1) streams or load strict float32 arrays."""
    base_seed, query_seed, train_seed = np.random.SeedSequence(args.seed).spawn(3)
    if args.base_file:
        base = load_vectors(args.base_file, "base")
        if args.dim is not None and base.shape[1] != args.dim:
            raise ValueError("base dimension does not match --dim")
    else:
        dimension = args.dim or 64
        base = np.random.default_rng(base_seed).random(
            (args.nb, dimension), dtype=np.float32
        )
    dimension = base.shape[1]
    if args.query_file:
        queries = load_vectors(args.query_file, "queries")
    else:
        queries = np.random.default_rng(query_seed).random(
            (args.nq, dimension), dtype=np.float32
        )
    queries = validate_vectors(queries, "queries", dimension)
    if args.train_file:
        training = validate_vectors(
            load_vectors(args.train_file, "training"), "training", dimension
        )
    elif args.train_size is not None:
        if args.train_size > len(base):
            raise ValueError("--train-size cannot exceed the database size")
        rows = np.random.default_rng(train_seed).choice(
            len(base), args.train_size, replace=False
        )
        training = np.ascontiguousarray(base[rows])
    else:
        training = base
    return Dataset(base, queries, training)


def validate_configuration(dataset, nlist, nprobes, k):
    dimension = validate_vectors(dataset.base, "base").shape[1]
    validate_vectors(dataset.queries, "queries", dimension)
    validate_vectors(dataset.training, "training", dimension)
    if nlist < 1 or len(dataset.training) < nlist:
        raise ValueError("nlist must be positive and <= the number of training vectors")
    if not nprobes or any(p < 1 or p > nlist for p in nprobes):
        raise ValueError("each nprobe must satisfy 1 <= nprobe <= nlist")
    if k < 1 or k > len(dataset.base):
        raise ValueError("k must satisfy 1 <= k <= database size")


def build_ivf(base, training, nlist, seed=42):
    """Time only FAISS training and insertion, separately."""
    base = validate_vectors(base, "base")
    training = validate_vectors(training, "training", base.shape[1])
    if nlist < 1 or len(training) < nlist:
        raise ValueError("need at least nlist training vectors")
    quantizer = faiss.IndexFlatL2(base.shape[1])
    index = faiss.IndexIVFFlat(quantizer, base.shape[1], nlist, faiss.METRIC_L2)
    index.cp.seed = seed
    if len(training) < index.cp.min_points_per_centroid * nlist:
        warnings.warn(
            f"Only {len(training)} training vectors for {nlist} lists; FAISS "
            f"recommends at least {index.cp.min_points_per_centroid * nlist}. "
            "Training is valid but clustering may be poor.",
            stacklevel=2,
        )
    start = time.perf_counter()
    index.train(training)
    train_seconds = time.perf_counter() - start
    start = time.perf_counter()
    index.add(base)
    add_seconds = time.perf_counter() - start
    return index, {"training_seconds": train_seconds, "insertion_seconds": add_seconds}


def inspect_clusters(index):
    """Read actual stored list membership, including empty lists."""
    if not index.is_trained:
        raise ValueError("index must be trained before inspection")
    centroids = index.quantizer.reconstruct_n(0, index.nlist)
    sizes = np.array(
        [index.invlists.list_size(i) for i in range(index.nlist)], dtype=np.int64
    )
    assignments = np.full(index.ntotal, -1, dtype=np.int64)
    for cluster, size in enumerate(sizes):
        if size:
            pointer = index.invlists.get_ids(cluster)
            try:
                ids = faiss.rev_swig_ptr(pointer, int(size)).copy()
            finally:
                index.invlists.release_ids(cluster, pointer)
            assignments[ids] = cluster
    if np.any(assignments < 0) or int(sizes.sum()) != index.ntotal:
        raise RuntimeError("inconsistent IVF list membership")
    return Clusters(centroids, assignments, sizes)


def build_exact(base):
    base = validate_vectors(base, "base")
    index = faiss.IndexFlatL2(base.shape[1])
    index.add(base)
    return index


def search_index(index, queries, k, repeats=3, warmup=1):
    """Benchmark batched FAISS calls; validation and warmup are outside timing."""
    queries = validate_vectors(queries, "queries", index.d)
    if not index.is_trained or not 1 <= k <= index.ntotal:
        raise ValueError("search requires a trained index and 1 <= k <= ntotal")
    if repeats < 1 or warmup < 0:
        raise ValueError("repeats must be positive and warmup nonnegative")
    for _ in range(warmup):
        index.search(queries, k)
    timings = []
    for _ in range(repeats):
        start = time.perf_counter()
        distances, ids = index.search(queries, k)
        timings.append(time.perf_counter() - start)
    return SearchResult(ids, distances, timings)


def recall_at_k(approximate, exact):
    """Mean set overlap / k; missing FAISS IDs (-1) never count as hits."""
    if approximate.shape != exact.shape or exact.ndim != 2 or not all(exact.shape):
        raise ValueError("recall requires matching, nonempty (nq, k) ID arrays")
    return float(np.mean([
        len(set(found[found >= 0]) & set(truth[truth >= 0])) / exact.shape[1]
        for found, truth in zip(approximate, exact)
    ]))


def export_experiment(directory, dataset, clusters, exact, ivf_results, report):
    """Keep a run self-contained for later locality and partitioning studies."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    for name, array in {
        "base": dataset.base, "queries": dataset.queries, "training": dataset.training,
        "centroids": clusters.centroids, "assignments": clusters.assignments,
        "cluster_sizes": clusters.sizes,
    }.items():
        np.save(directory / f"{name}.npy", array, allow_pickle=False)
    np.savez(directory / "exact_results.npz", ids=exact.ids, squared_l2=exact.distances)
    for nprobe, result in ivf_results.items():
        np.savez(directory / f"ivf_nprobe_{nprobe}.npz",
                 ids=result.ids, squared_l2=result.distances)
    with (directory / "cluster_sizes.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["cluster_id", "size"])
        writer.writerows(enumerate(clusters.sizes.tolist()))
    with (directory / "summary.json").open("w") as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write("\n")


def make_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dim", type=positive_int, help="dimension (default: 64, or loaded base dimension)")
    parser.add_argument("--nb", type=positive_int, default=10000, help="generated base count (default: 10000)")
    parser.add_argument("--nq", type=positive_int, default=100, help="generated query count (default: 100)")
    parser.add_argument("--seed", type=nonnegative_int, default=42)
    parser.add_argument("--base-file", type=Path)
    parser.add_argument("--query-file", type=Path)
    training = parser.add_mutually_exclusive_group()
    training.add_argument("--train-file", type=Path)
    training.add_argument("--train-size", type=positive_int, help="sample this many base rows without replacement")
    parser.add_argument("--nlist", type=positive_int, default=64)
    parser.add_argument("--nprobe", type=positive_int, nargs="+", default=[1, 4, 16, 64])
    parser.add_argument("--k", type=positive_int, default=10)
    parser.add_argument("--threads", type=positive_int, default=1)
    parser.add_argument("--repeats", type=positive_int, default=3)
    parser.add_argument("--warmup", type=nonnegative_int, default=1)
    parser.add_argument("--examples", type=nonnegative_int, default=2)
    parser.add_argument("--output-dir", type=Path, help="optional directory for arrays, results and metadata")
    return parser


def run(args):
    # FAISS clustering uses a signed 32-bit seed; NumPy accepts larger integers.
    if args.seed > 2**31 - 1:
        raise ValueError("--seed must be <= 2147483647 for FAISS")
    faiss.omp_set_num_threads(args.threads)
    dataset = prepare_dataset(args)
    nprobes = list(dict.fromkeys(args.nprobe))
    validate_configuration(dataset, args.nlist, nprobes, args.k)
    index, build_times = build_ivf(dataset.base, dataset.training, args.nlist, args.seed)
    clusters = inspect_clusters(index)
    exact = search_index(build_exact(dataset.base), dataset.queries, args.k,
                         args.repeats, args.warmup)
    report = {
        "configuration": {key: str(value) if isinstance(value, Path) else value
                          for key, value in vars(args).items()},
        "environment": {
            "python": platform.python_version(), "numpy": np.__version__,
            "faiss": faiss.__version__, "platform": platform.platform(),
            "machine": platform.machine(), "processor": platform.processor(),
            "faiss_threads": faiss.omp_get_max_threads(),
            "faiss_compile_options": faiss.get_compile_options(),
        },
        "dataset": {"nb": len(dataset.base), "nq": len(dataset.queries),
                    "dimension": dataset.base.shape[1], "training_count": len(dataset.training)},
        "index": {"is_trained": bool(index.is_trained), "ntotal": index.ntotal,
                  "clustering_seed": int(index.cp.seed),
                  "clustering_iterations": int(index.cp.niter),
                  "max_points_per_centroid": int(index.cp.max_points_per_centroid)},
        "build": build_times, "clusters": clusters.summary(),
        "exact": exact.metrics(), "ivf": [],
        "measurement": "Native CPU wall-clock seconds via time.perf_counter; batched search, mean over repeats; warmup excluded.",
    }
    print(f"Dataset: nb={len(dataset.base)}, nq={len(dataset.queries)}, "
          f"dim={dataset.base.shape[1]}, training={len(dataset.training)}, threads={args.threads}")
    print(f"IVF trained={index.is_trained}, ntotal={index.ntotal}, nlist={index.nlist}")
    print(f"Training: {build_times['training_seconds']:.6f} s; "
          f"insertion: {build_times['insertion_seconds']:.6f} s")
    print(f"Cluster sizes (including empty): {clusters.summary()}")
    print("Vectors per cluster:", clusters.sizes.tolist())
    print("Search times: mean batch time; latency = batch time / query count")
    print(f"{'index':<18} {'Recall@k':>10} {'seconds':>12} {'ms/query':>12} {'QPS':>12}")
    def print_metrics(label, metrics, recall=None):
        recall_text = "-" if recall is None else f"{recall:.6f}"
        print(f"{label:<18} {recall_text:>10} {metrics['search_seconds']:12.6f} "
              f"{metrics['latency_ms_per_query']:12.6f} {metrics['queries_per_second']:12.1f}")
    print_metrics("Flat (exact)", report["exact"])
    results = {}
    for nprobe in nprobes:
        index.nprobe = nprobe
        result = search_index(index, dataset.queries, args.k, args.repeats, args.warmup)
        recall = recall_at_k(result.ids, exact.ids)
        metrics = {"nprobe": nprobe, "recall_at_k": recall, **result.metrics()}
        report["ivf"].append(metrics)
        results[nprobe] = result
        print_metrics(f"IVF nprobe={nprobe}", metrics, recall)
    for query in range(min(args.examples, len(dataset.queries))):
        print(f"Query {query} exact IDs: {exact.ids[query].tolist()}")
        print(f"  exact squared L2: {exact.distances[query].tolist()}")
        for nprobe, result in results.items():
            print(f"  nprobe={nprobe} IDs: {result.ids[query].tolist()}")
            print(f"    squared L2: {result.distances[query].tolist()}")
    if any(row["missing_neighbors"] for row in report["ivf"]):
        print("Some probed lists contain fewer than k candidates: ID -1 denotes a missing neighbor; its distance is a FAISS sentinel.")
    if args.output_dir:
        export_experiment(args.output_dir, dataset, clusters, exact, results, report)
        print(f"Exported experiment to {args.output_dir}")
    return report


def main(argv=None):
    parser = make_parser()
    args = parser.parse_args(argv)
    try:
        run(args)
    except (ValueError, OSError, RuntimeError) as error:
        parser.exit(2, f"error: {error}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
