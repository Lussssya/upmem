import tempfile
import unittest
from decimal import Decimal
from pathlib import Path

from upimulator.engine.engine import DistributionEngine
from upimulator.engine.input_data import MAX_ELEMENTS, load_values
from upimulator.engine.models import BackendResult


class FakeDpuBackend:
    def __init__(self):
        self.calls: list[tuple[int, ...]] = []

    def execute(self, values):
        values = tuple(values)
        self.calls.append(values)
        return BackendResult(partial_sum=sum(values), simulated_cycles=123 if values else None)


class DistributionEngineTests(unittest.TestCase):
    def setUp(self):
        self.dpu = FakeDpuBackend()
        self.engine = DistributionEngine(self.dpu)
        self.values = (17, 3, 99, 0, 42, 8, 1000, 7, 11, 250)

    def test_cpu_only(self):
        result = self.engine.execute(self.values, "cpu")
        self.assertTrue(result.correct)
        self.assertEqual(result.cpu_elements, 10)
        self.assertEqual(result.dpu_elements, 0)
        self.assertEqual(self.dpu.calls, [()])

    def test_dpu_only(self):
        result = self.engine.execute(self.values, "dpu")
        self.assertTrue(result.correct)
        self.assertEqual(result.cpu_elements, 0)
        self.assertEqual(result.dpu_elements, 10)
        self.assertEqual(self.dpu.calls, [self.values])

    def test_hybrid_uses_prefix_and_suffix(self):
        result = self.engine.execute(self.values, "hybrid", Decimal("0.4"))
        self.assertTrue(result.correct)
        self.assertEqual(result.cpu_elements, 4)
        self.assertEqual(result.dpu_elements, 6)
        self.assertEqual(self.dpu.calls, [self.values[4:]])

    def test_hybrid_zero_cpu_share(self):
        result = self.engine.execute(self.values, "hybrid", Decimal("0"))
        self.assertTrue(result.correct)
        self.assertEqual(result.cpu_elements, 0)
        self.assertEqual(self.dpu.calls, [self.values])

    def test_hybrid_full_cpu_share(self):
        result = self.engine.execute(self.values, "hybrid", Decimal("1"))
        self.assertTrue(result.correct)
        self.assertEqual(result.dpu_elements, 0)
        self.assertEqual(self.dpu.calls, [()])

    def test_hybrid_rejects_share_above_one(self):
        with self.assertRaisesRegex(ValueError, "between 0 and 1"):
            self.engine.execute(self.values, "hybrid", Decimal("1.1"))


class InputDataTests(unittest.TestCase):
    def write_input(self, contents: str) -> Path:
        temporary = tempfile.NamedTemporaryFile(mode="w", delete=False)
        self.addCleanup(Path(temporary.name).unlink, missing_ok=True)
        temporary.write(contents)
        temporary.close()
        return Path(temporary.name)

    def test_arbitrary_values_comments_commas_and_whitespace(self):
        path = self.write_input("5, 9\n# comment\n2 100 7\n")
        self.assertEqual(load_values(path), (5, 9, 2, 100, 7))

    def test_rejects_more_than_capacity(self):
        path = self.write_input(" ".join("1" for _ in range(MAX_ELEMENTS + 1)))
        with self.assertRaisesRegex(ValueError, "at most 256"):
            load_values(path)

    def test_accepts_exact_capacity(self):
        path = self.write_input(" ".join("1" for _ in range(MAX_ELEMENTS)))
        self.assertEqual(len(load_values(path)), MAX_ELEMENTS)

    def test_rejects_empty_input(self):
        path = self.write_input("# no values\n")
        with self.assertRaisesRegex(ValueError, "at least one"):
            load_values(path)

    def test_rejects_negative_values(self):
        path = self.write_input("1 -2 3")
        with self.assertRaisesRegex(ValueError, "outside uint32"):
            load_values(path)

    def test_rejects_sum_overflow(self):
        path = self.write_input("4294967295 1")
        with self.assertRaisesRegex(ValueError, "sum must fit"):
            load_values(path)


if __name__ == "__main__":
    unittest.main()
