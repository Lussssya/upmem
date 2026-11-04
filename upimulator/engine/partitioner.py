from decimal import Decimal
from typing import Sequence

from .models import Partition


class Partitioner:
    """Assign a prefix to the CPU and the remaining suffix to the DPU."""

    def partition(
        self,
        values: Sequence[int],
        mode: str,
        cpu_share: Decimal | None,
    ) -> Partition:
        if mode == "cpu":
            cpu_count = len(values)
        elif mode == "dpu":
            cpu_count = 0
        elif mode == "hybrid":
            if cpu_share is None:
                raise ValueError("--cpu-share is required in hybrid mode")
            if cpu_share < 0 or cpu_share > 1:
                raise ValueError("CPU share must be between 0 and 1")
            cpu_count = int(Decimal(len(values)) * cpu_share)
        else:
            raise ValueError(f"unsupported execution mode: {mode}")

        return Partition(
            cpu_values=tuple(values[:cpu_count]),
            dpu_values=tuple(values[cpu_count:]),
        )
