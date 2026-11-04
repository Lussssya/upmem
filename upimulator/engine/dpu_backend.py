import os
import subprocess
from pathlib import Path
from typing import Sequence

from .models import BackendResult


class DpuBackend:
    """Run the assigned values through the installed Go uPIMulator."""

    def __init__(self, upimulator_root: Path):
        self.upimulator_root = upimulator_root.expanduser().resolve()
        self.runner = self.upimulator_root / "run_distributed_sum.sh"
        self.output_file = (
            self.upimulator_root / "bin" / "distributed_sum" / "actual_output.bin"
        )
        self.log_file = self.upimulator_root / "bin" / "distributed_sum" / "log.txt"

    def execute(self, values: Sequence[int]) -> BackendResult:
        if not values:
            return BackendResult(partial_sum=0)
        if not self.runner.is_file():
            raise RuntimeError(
                f"DPU backend is not installed: missing {self.runner}. "
                "Run upimulator/install.sh first."
            )

        completed = subprocess.run(
            [str(self.runner), ",".join(str(value) for value in values)],
            cwd=self.upimulator_root,
            env=os.environ.copy(),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode != 0:
            details = "\n".join(
                section
                for section in (completed.stdout.strip(), completed.stderr.strip())
                if section
            )
            raise RuntimeError(f"uPIMulator DPU execution failed:\n{details}")

        partial_sum = self._read_uint32(self.output_file)
        simulated_cycles = self._read_logic_cycles(self.log_file)
        return BackendResult(
            partial_sum=partial_sum,
            simulated_cycles=simulated_cycles,
        )

    @staticmethod
    def _read_uint32(path: Path) -> int:
        if not path.is_file():
            raise RuntimeError(f"missing captured DPU output: {path}")

        try:
            data = [int(line.strip()) for line in path.read_text().splitlines()]
        except ValueError as error:
            raise RuntimeError(f"invalid captured DPU output: {path}") from error

        if len(data) != 4 or any(byte < 0 or byte > 255 for byte in data):
            raise RuntimeError(f"captured DPU output is not one uint32: {path}")

        return sum(byte << (8 * index) for index, byte in enumerate(data))

    @staticmethod
    def _read_logic_cycles(path: Path) -> int:
        if not path.is_file():
            raise RuntimeError(f"missing uPIMulator statistics: {path}")

        key = "Logic[0_0_0]_logic_cycle"
        for line in path.read_text().splitlines():
            name, separator, value = line.partition(": ")
            if separator and name == key:
                return int(value)
        raise RuntimeError(f"{key} was not found in {path}")
