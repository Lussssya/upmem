"""Run with python -m unittest discover -s tests -v (no test framework needed)."""

import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import faiss
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ivf_flat as app


class IvfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        faiss.omp_set_num_threads(1)
        rng = np.random.default_rng(123)
        cls.base = rng.random((512, 8), dtype=np.float32)
        cls.queries = rng.random((11, 8), dtype=np.float32)
        cls.index, cls.build_times = app.build_ivf(cls.base, cls.base, 4, 123)

    def test_index_construction(self):
        self.assertTrue(self.index.is_trained)
        self.assertEqual(self.index.ntotal, len(self.base))
        self.assertEqual(self.index.nlist, 4)
        self.assertEqual(self.index.metric_type, faiss.METRIC_L2)
        self.assertEqual(self.index.code_size, self.base.shape[1] * 4)
        self.assertGreaterEqual(self.build_times["training_seconds"], 0)
        self.assertGreaterEqual(self.build_times["insertion_seconds"], 0)

    def test_cluster_assignments_and_centroids(self):
        clusters = app.inspect_clusters(self.index)
        self.assertEqual(clusters.centroids.shape, (4, 8))
        self.assertEqual(clusters.centroids.dtype, np.float32)
        self.assertEqual(clusters.assignments.shape, (len(self.base),))
        self.assertEqual(int(clusters.sizes.sum()), len(self.base))
        np.testing.assert_array_equal(
            np.bincount(clusters.assignments, minlength=4), clusters.sizes
        )
        _, expected = self.index.quantizer.search(self.base, 1)
        np.testing.assert_array_equal(clusters.assignments, expected[:, 0])

    def test_empty_clusters_in_summary(self):
        # Train on a broad distribution, but add only one vector to one list.
        index, _ = app.build_ivf(self.base[:1], self.base, 4)
        clusters = app.inspect_clusters(index)
        self.assertEqual(clusters.summary()["empty"], 3)
        self.assertEqual(clusters.summary()["minimum"], 0)
        self.assertEqual(clusters.summary()["maximum"], 1)
        self.assertEqual(clusters.summary()["average"], 0.25)

    def test_all_lists_equal_exact_search(self):
        self.index.nprobe = self.index.nlist
        for queries, k in [(self.queries, 7), (self.queries[:1], 1),
                           (self.queries[:1], len(self.base))]:
            with self.subTest(nq=len(queries), k=k):
                ivf = app.search_index(self.index, queries, k, repeats=1, warmup=0)
                exact = app.search_index(app.build_exact(self.base), queries, k,
                                         repeats=1, warmup=0)
                self.assertEqual(ivf.ids.shape, (len(queries), k))
                self.assertEqual(ivf.distances.shape, (len(queries), k))
                np.testing.assert_array_equal(ivf.ids, exact.ids)
                np.testing.assert_allclose(ivf.distances, exact.distances,
                                           rtol=1e-5, atol=1e-6)
                self.assertEqual(app.recall_at_k(ivf.ids, exact.ids), 1.0)

    def test_known_nearest_neighbor_and_squared_distance(self):
        base = np.array([[0, 0], [3, 0], [0, 4], [8, 8]], dtype=np.float32)
        index, _ = app.build_ivf(base, np.tile(base, (40, 1)), 1)
        query = np.array([[0, 1]], dtype=np.float32)
        result = app.search_index(index, query, 3, repeats=1, warmup=0)
        np.testing.assert_array_equal(result.ids, [[0, 2, 1]])
        np.testing.assert_allclose(result.distances, [[1, 9, 10]])

    def test_missing_neighbors_and_recall(self):
        index, _ = app.build_ivf(self.base[:4], self.base, 4)
        index.nprobe = 1
        result = app.search_index(index, self.queries, 4, repeats=1, warmup=0)
        self.assertTrue(np.any(result.ids == -1))
        self.assertGreater(result.metrics()["missing_neighbors"], 0)
        found = np.array([[1, 1, -1], [2, 7, -1]])
        truth = np.array([[1, 2, 3], [2, 3, 4]])
        self.assertAlmostEqual(app.recall_at_k(found, truth), 1 / 3)

    def test_reproducibility(self):
        parser = app.make_parser()
        args = parser.parse_args(["--nb", "200", "--nq", "3", "--dim", "8",
                                  "--train-size", "160", "--seed", "123"])
        first = app.prepare_dataset(args)
        second = app.prepare_dataset(args)
        for name in ("base", "queries", "training"):
            np.testing.assert_array_equal(getattr(first, name), getattr(second, name))
        self.assertFalse(np.array_equal(first.base[:3], first.queries))
        duplicate, _ = app.build_ivf(self.base, self.base, 4, 123)
        np.testing.assert_array_equal(app.inspect_clusters(self.index).centroids,
                                      app.inspect_clusters(duplicate).centroids)

    def test_invalid_vectors(self):
        for vectors in [np.zeros((2, 8), dtype=np.float64),
                        np.zeros((2, 8), dtype=np.int32),
                        np.zeros(8, dtype=np.float32),
                        np.empty((0, 8), dtype=np.float32),
                        np.empty((2, 0), dtype=np.float32),
                        np.full((2, 8), np.nan, dtype=np.float32),
                        np.full((2, 8), np.inf, dtype=np.float32),
                        np.zeros((2, 7), dtype=np.float32)]:
            with self.subTest(shape=vectors.shape, dtype=vectors.dtype):
                with self.assertRaises(ValueError):
                    app.validate_vectors(vectors, "test", 8)
        noncontiguous = self.base[:, ::-1]
        self.assertTrue(app.validate_vectors(noncontiguous, "test").flags.c_contiguous)

    def test_invalid_configurations(self):
        dataset = app.Dataset(self.base, self.queries, self.base)
        for nlist, probes, k in [(0, [1], 1), (513, [1], 1), (4, [5], 1),
                                 (4, [0], 1), (4, [], 1), (4, [1], 513),
                                 (4, [1], 0)]:
            with self.subTest(nlist=nlist, nprobes=probes, k=k):
                with self.assertRaises(ValueError):
                    app.validate_configuration(dataset, nlist, probes, k)
        with self.assertRaises(ValueError):
            app.build_ivf(self.base, self.base[:3], 4)

    def test_insufficient_training_warning(self):
        with self.assertWarns(UserWarning):
            index, _ = app.build_ivf(self.base[:2], self.base[:2], 1)
        self.assertTrue(index.is_trained)

    def test_loading_and_export_roundtrip(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            np.save(root / "base.npy", self.base)
            np.save(root / "query.npy", self.queries[:1])
            np.save(root / "training.npy", self.base[:200])
            args = app.make_parser().parse_args([
                "--base-file", str(root / "base.npy"),
                "--query-file", str(root / "query.npy"),
                "--train-file", str(root / "training.npy"),
                "--nlist", "4", "--nprobe", "1", "4", "--k", "5",
                "--repeats", "1", "--examples", "0",
                "--output-dir", str(root / "export"),
            ])
            with contextlib.redirect_stdout(io.StringIO()):
                report = app.run(args)
            out = root / "export"
            stored = json.loads((out / "summary.json").read_text())
            self.assertEqual(stored["dataset"]["nq"], 1)
            self.assertEqual(stored["dataset"]["dimension"], 8)
            self.assertEqual(stored["ivf"][-1]["recall_at_k"], 1.0)
            self.assertEqual(report["environment"]["faiss_threads"], 1)
            for name, expected in [("base", self.base), ("queries", self.queries[:1]),
                                   ("training", self.base[:200])]:
                np.testing.assert_array_equal(np.load(out / f"{name}.npy"), expected)
            sizes = np.load(out / "cluster_sizes.npy")
            assignments = np.load(out / "assignments.npy")
            np.testing.assert_array_equal(np.bincount(assignments, minlength=4), sizes)
            self.assertEqual(np.load(out / "centroids.npy").shape, (4, 8))
            csv_sizes = np.loadtxt(out / "cluster_sizes.csv", delimiter=",", skiprows=1)
            np.testing.assert_array_equal(csv_sizes[:, 1], sizes)
            with np.load(out / "exact_results.npz") as exact:
                with np.load(out / "ivf_nprobe_4.npz") as ivf:
                    np.testing.assert_array_equal(exact["ids"], ivf["ids"])
                    np.testing.assert_allclose(exact["squared_l2"], ivf["squared_l2"], atol=1e-6)
            with np.load(out / "ivf_nprobe_1.npz") as ivf:
                self.assertEqual(ivf["ids"].shape, (1, 5))

    def test_invalid_loaded_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            parser = app.make_parser()
            np.save(root / "base.npy", self.base)
            np.save(root / "wrong.npy", np.zeros((1, 9), dtype=np.float32))
            for extra in [["--dim", "9"], ["--query-file", str(root / "wrong.npy")],
                          ["--train-file", str(root / "wrong.npy")],
                          ["--train-size", "513"]]:
                with self.subTest(extra=extra):
                    args = parser.parse_args(["--base-file", str(root / "base.npy"), *extra])
                    with self.assertRaises(ValueError):
                        app.prepare_dataset(args)
            (root / "broken.npy").write_bytes(b"invalid")
            with self.assertRaises(ValueError):
                app.load_vectors(root / "broken.npy", "test")

    def test_benchmark_metrics(self):
        result = app.search_index(app.build_exact(self.base), self.queries, 2,
                                  repeats=2, warmup=1)
        metrics = result.metrics()
        self.assertEqual(len(result.seconds_per_run), 2)
        self.assertGreater(metrics["search_seconds"], 0)
        self.assertAlmostEqual(metrics["latency_ms_per_query"],
                               metrics["search_seconds"] * 1000 / len(self.queries))
        self.assertAlmostEqual(metrics["queries_per_second"],
                               len(self.queries) / metrics["search_seconds"])

    def test_cli_errors_are_actionable(self):
        script = Path(app.__file__)
        for arguments, message in [(["--nlist", "4", "--nprobe", "5"], "nprobe"),
                                   (["--nb", "3", "--nlist", "4", "--nprobe", "1"], "training"),
                                   (["--nb", "3", "--nlist", "1", "--nprobe", "1", "--k", "4"], "k must"),
                                   (["--threads", "0"], "positive integer"),
                                   (["--seed", "2147483648"], "seed"),
                                   (["--base-file", "/missing/base.npy"], "No such file")]:
            with self.subTest(arguments=arguments):
                process = subprocess.run([sys.executable, str(script), *arguments],
                                         text=True, capture_output=True)
                self.assertEqual(process.returncode, 2)
                self.assertIn(message, process.stderr)
                self.assertNotIn("Traceback", process.stderr)


if __name__ == "__main__":
    unittest.main()
