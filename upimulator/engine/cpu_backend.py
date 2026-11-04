from typing import Sequence

from .models import BackendResult


class CpuBackend:
    """Execute an array sum natively in the Python host process."""

    def execute(self, values: Sequence[int]) -> BackendResult:
        return BackendResult(partial_sum=sum(values))
