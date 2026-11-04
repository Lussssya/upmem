from decimal import Decimal
from typing import Sequence

from .cpu_backend import CpuBackend
from .models import BackendResult, ExecutionResult
from .partitioner import Partitioner


class ResultCollector:
    def collect(
        self,
        mode: str,
        values: Sequence[int],
        cpu_values: Sequence[int],
        dpu_values: Sequence[int],
        cpu_result: BackendResult,
        dpu_result: BackendResult,
    ) -> ExecutionResult:
        combined = cpu_result.partial_sum + dpu_result.partial_sum
        reference = sum(values)
        return ExecutionResult(
            mode=mode,
            total_elements=len(values),
            cpu_elements=len(cpu_values),
            dpu_elements=len(dpu_values),
            cpu_partial_sum=cpu_result.partial_sum,
            dpu_partial_sum=dpu_result.partial_sum,
            combined_result=combined,
            reference_result=reference,
            correct=combined == reference,
            dpu_simulated_cycles=dpu_result.simulated_cycles,
        )


class DistributionEngine:
    def __init__(self, dpu_backend):
        self.partitioner = Partitioner()
        self.cpu_backend = CpuBackend()
        self.dpu_backend = dpu_backend
        self.collector = ResultCollector()

    def execute(
        self,
        values: Sequence[int],
        mode: str,
        cpu_share: Decimal | None = None,
    ) -> ExecutionResult:
        partition = self.partitioner.partition(values, mode, cpu_share)
        cpu_result = self.cpu_backend.execute(partition.cpu_values)
        dpu_result = self.dpu_backend.execute(partition.dpu_values)
        return self.collector.collect(
            mode=mode,
            values=values,
            cpu_values=partition.cpu_values,
            dpu_values=partition.dpu_values,
            cpu_result=cpu_result,
            dpu_result=dpu_result,
        )
