from dataclasses import dataclass
from typing import Optional, Sequence


@dataclass(frozen=True)
class Partition:
    cpu_values: Sequence[int]
    dpu_values: Sequence[int]


@dataclass(frozen=True)
class BackendResult:
    partial_sum: int
    simulated_cycles: Optional[int] = None


@dataclass(frozen=True)
class ExecutionResult:
    mode: str
    total_elements: int
    cpu_elements: int
    dpu_elements: int
    cpu_partial_sum: int
    dpu_partial_sum: int
    combined_result: int
    reference_result: int
    correct: bool
    dpu_simulated_cycles: Optional[int]
