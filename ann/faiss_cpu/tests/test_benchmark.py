"""Statistics, workload invariants, repeated timing and benchmark export checks."""

import contextlib
import csv
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

import faiss
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
try:
    import benchmark as experiment
    import matplotlib
except ModuleNotFoundError as error:
    raise unittest.SkipTest("Install benchmark-requirements.txt to run benchmark tests") from error
import ivf_flat as baseline


class BenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        faiss.omp_set_num_threads(1)
        rng = np.random.default_rng(98)
        cls.base = rng.random((400, 8), dtype=np.float32)
        cls.queries = rng.random((9, 8), dtype=np.float32)
        cls.index, _ = baseline.build_ivf(cls.base, cls.base, 4, 98)
        cls.clusters = baseline.inspect_clusters(cls.index)

    def test_cluster_statistics_include_zeros(self):
        sizes = np.array([0, 2, 4, 6], dtype=np.int64)
        stats = experiment.cluster_statistics(sizes, 12)
        self.assertEqual(stats["total_vectors"], 12)
        self.assertEqual(stats["nlist"], 4)
        self.assertEqual(stats["minimum"], 0)
        self.assertEqual(stats["maximum"], 6)
        self.assertEqual(stats["mean"], 3)
        self.assertAlmostEqual(stats["std_population"], np.sqrt(5))
        self.assertEqual(stats["empty_clusters"], 1)
        self.assertEqual(stats["largest_to_mean_ratio"], 2)

    def test_sizes_sum_to_ntotal(self):
        stats = experiment.cluster_statistics(self.clusters.sizes, self.index.ntotal)
        self.assertEqual(stats["total_vectors"], len(self.base))
        self.assertEqual(int(self.clusters.sizes.sum()), self.index.ntotal)
        with self.assertRaisesRegex(ValueError, "sum"):
            experiment.cluster_statistics(self.clusters.sizes, self.index.ntotal + 1)

    def test_bad_cluster_statistics(self):
        for sizes, total in [([], 1), ([0, 0], 0), ([-1, 3], 2),
                             ([1.5, 2.5], 4), ([[1, 2]], 3)]:
            with self.subTest(sizes=sizes):
                with self.assertRaises(ValueError):
                    experiment.cluster_statistics(sizes, total)

    def test_known_workload_statistics(self):
        sizes = np.array([0, 2, 4, 6], dtype=np.int64)
        selected = np.array([[1, 2], [1, 3], [1, 3]], dtype=np.int64)
        work = experiment.summarize_workload(selected, sizes)
        np.testing.assert_array_equal(work.query_counts, [0, 3, 1, 2])
        np.testing.assert_array_equal(work.estimated_scan_vectors, [6, 8, 8])
        summary = work.summary
        self.assertEqual(summary["total_cluster_selections"], 6)
        self.assertEqual(summary["estimated_scan_vectors_total"], 22)
        self.assertAlmostEqual(summary["estimated_scan_vectors_mean_per_query"], 22 / 3)
        self.assertEqual(summary["traffic_mean"], 1.5)
        self.assertAlmostEqual(summary["traffic_std_population"], np.sqrt(1.25))
        self.assertAlmostEqual(summary["traffic_cv"], np.sqrt(1.25) / 1.5)
        self.assertEqual(summary["traffic_max_to_mean_ratio"], 2)
        self.assertEqual(summary["unselected_clusters"], 1)
        self.assertEqual(summary["clusters_at_least_2x_mean_traffic"], 1)

    def test_quantizer_selections_and_work_estimates(self):
        for nprobe in [1, 2, 4]:
            with self.subTest(nprobe=nprobe):
                work = experiment.analyze_queries(self.index, self.queries,
                                                  self.clusters.sizes, nprobe)
                _, expected = self.index.quantizer.search(self.queries, nprobe)
                np.testing.assert_array_equal(work.selected_clusters, expected)
                self.assertEqual(int(work.query_counts.sum()), len(self.queries) * nprobe)
                for row, chosen in enumerate(work.selected_clusters):
                    count = np.count_nonzero(np.isin(self.clusters.assignments, chosen))
                    self.assertEqual(int(work.estimated_scan_vectors[row]), count)
                    self.assertEqual(len(set(chosen)), nprobe)
                self.assertEqual(work.summary["estimated_scan_vectors_total"],
                                 int(np.dot(work.query_counts, self.clusters.sizes)))

    def test_all_clusters_selected_balances_traffic(self):
        work = experiment.analyze_queries(self.index, self.queries, self.clusters.sizes, 4)
        np.testing.assert_array_equal(work.query_counts, np.full(4, len(self.queries)))
        np.testing.assert_array_equal(work.estimated_scan_vectors,
                                      np.full(len(self.queries), len(self.base)))
        self.assertEqual(work.summary["traffic_cv"], 0)
        self.assertEqual(work.summary["traffic_max_to_mean_ratio"], 1)

    def test_invalid_workload_inputs(self):
        sizes = np.array([0, 2, 4, 6], dtype=np.int64)
        for selected in [[], [[-1]], [[4]], [[1.5]], [[1, 1]], np.empty((1, 0), dtype=np.int64)]:
            with self.subTest(selected=selected):
                with self.assertRaises(ValueError):
                    experiment.summarize_workload(np.asarray(selected), sizes)
        for nprobe in [0, 5]:
            with self.assertRaises(ValueError):
                experiment.analyze_queries(self.index, self.queries, self.clusters.sizes, nprobe)

    def test_known_timing_statistics(self):
        stats = experiment.timing_statistics([1, 2, 3, 4, 5], 1000)
        self.assertEqual(stats["mean_latency_ms_per_query"], 3)
        self.assertEqual(stats["median_latency_ms_per_query"], 3)
        self.assertEqual(stats["latency_q25_ms"], 2)
        self.assertEqual(stats["latency_q75_ms"], 4)
        self.assertEqual(stats["latency_iqr_ms"], 2)
        self.assertAlmostEqual(stats["latency_std_sample_ms"], np.sqrt(2.5))
        self.assertAlmostEqual(stats["qps_from_median"], 1000 / 3)
        for seconds in [[1, 2, 3], [0, 1, 2, 3, 4], [1, 2, 3, 4, np.nan]]:
            with self.assertRaises(ValueError):
                experiment.timing_statistics(seconds, 1)

    def test_benchmark_rounds_and_correctness(self):
        rows, repetitions, results = experiment.benchmark_searches(
            self.index, baseline.build_exact(self.base), self.queries, 5,
            [1, 4], repeats=5, warmup=1, seed=7, progress=lambda *args, **kwargs: None,
        )
        self.assertEqual(len(repetitions), 15)
        for repeat in range(1, 6):
            cases = [row["nprobe"] for row in repetitions if row["round"] == repeat]
            self.assertCountEqual(cases, ["", 1, 4])
        np.testing.assert_array_equal(results[0].ids, results[4].ids)
        self.assertEqual(rows[-1]["recall_at_k"], 1)
        self.assertEqual(rows[0]["speedup_vs_flat_median"], 1)
        self.assertEqual(rows[0]["speedup_vs_flat_mean"], 1)
        for row in rows:
            self.assertEqual(row["measured_repetitions"], 5)
            self.assertAlmostEqual(row["speedup_vs_flat_median"],
                                   rows[0]["median_search_seconds"] / row["median_search_seconds"])

    def test_default_configuration_and_cli_overrides(self):
        args = experiment.parse_args([])
        self.assertEqual((args.nb, args.dim, args.nq, args.nlist, args.k),
                         (100000, 128, 1000, 128, 10))
        self.assertEqual(args.nprobe, [1, 4, 8, 16, 32, 64, 128])
        self.assertEqual((args.repeats, args.warmup, args.threads), (5, 2, 1))
        self.assertEqual(args.examples, 0)
        self.assertEqual(baseline.make_parser().parse_args([]).examples, 2)
        args = experiment.parse_args(["--nb", "400", "--dim", "8", "--nprobe", "1", "4", "--threads", "2"])
        self.assertEqual((args.nb, args.dim, args.threads), (400, 8, 2))
        self.assertEqual(args.nprobe, [1, 4])

    def test_bad_benchmark_configuration(self):
        for arguments in [["--repeats", "4"], ["--warmup", "0"], ["--threads", "0"]]:
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    experiment.parse_args(arguments)
                self.assertEqual(error.exception.code, 2)
        with tempfile.TemporaryDirectory() as temporary:
            config = Path(temporary) / "bad.json"
            for data in [{"typo": 1}, {"nprobe": 1}, {"nb": 1.5}, {"no_plots": 1}, []]:
                config.write_text(json.dumps(data))
                with self.subTest(data=data), contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit):
                        experiment.parse_args(["--config", str(config)])

    def test_end_to_end_exports_and_figures(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "benchmark"
            args = experiment.parse_args([
                "--nb", "400", "--nq", "7", "--dim", "8", "--nlist", "4",
                "--nprobe", "1", "2", "4", "--k", "5", "--warmup", "1",
                "--examples", "1",
                "--output-dir", str(out),
            ])
            console = io.StringIO()
            with contextlib.redirect_stdout(console):
                report = experiment.run(args)
            self.assertIn("Query 0:", console.getvalue())
            def read_csv(name):
                with (out / name).open() as stream:
                    return list(csv.DictReader(stream))
            self.assertEqual(report["clusters"]["total_vectors"], 400)
            self.assertEqual(report["benchmarks"][-1]["recall_at_k"], 1)
            self.assertEqual(sum(int(row["size"]) for row in read_csv("cluster_sizes.csv")), 400)
            self.assertEqual(len(read_csv("benchmark_results.csv")), 4)
            self.assertEqual(len(read_csv("search_repetitions.csv")), 20)
            self.assertEqual(len(read_csv("query_workload.csv")), 21)
            self.assertEqual(len(read_csv("query_selected_lists.csv")), 7 * (1 + 2 + 4))
            self.assertEqual(len(read_csv("cluster_query_traffic.csv")), 12)
            self.assertEqual(len(read_csv("workload_summary.csv")), 3)
            self.assertEqual(int(read_csv("cluster_statistics.csv")[0]["total_vectors"]), 400)
            stored = json.loads((out / "summary.json").read_text())
            self.assertIn("not an instrumented", stored["measurement"]["scan_workload"])
            self.assertEqual(stored["environment"]["faiss_threads"], 1)
            for pool in stored["environment"]["thread_pools"]:
                self.assertEqual(pool["num_threads"], 1)
            for figure in ["cluster_sizes.png", "recall_latency.png"]:
                self.assertTrue((out / figure).read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))
            for filename in ["base.npy", "queries.npy", "training.npy", "centroids.npy",
                             "assignments.npy", "cluster_sizes.npy", "exact_results.npz",
                             "ivf_nprobe_1.npz", "ivf_nprobe_2.npz", "ivf_nprobe_4.npz"]:
                self.assertTrue((out / filename).is_file())


if __name__ == "__main__":
    unittest.main()
