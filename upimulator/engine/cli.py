import argparse
import os
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path

from .dpu_backend import DpuBackend
from .engine import DistributionEngine
from .input_data import load_values


def cpu_share(value: str) -> Decimal:
    try:
        share = Decimal(value)
    except InvalidOperation as error:
        raise argparse.ArgumentTypeError("CPU share must be a number") from error
    if not share.is_finite() or share < 0 or share > 1:
        raise argparse.ArgumentTypeError("CPU share must be between 0 and 1")
    return share


def build_parser() -> argparse.ArgumentParser:
    default_root = os.environ.get(
        "UPIMULATOR_ROOT",
        "~/pim/upimulator/golang/uPIMulator",
    )
    parser = argparse.ArgumentParser(
        description="Distribute an array sum between the native CPU and uPIMulator",
    )
    parser.add_argument("--mode", required=True, choices=("cpu", "dpu", "hybrid"))
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--cpu-share", type=cpu_share)
    parser.add_argument(
        "--upimulator-root",
        type=Path,
        default=Path(default_root),
        help="installed Go uPIMulator root",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.mode == "hybrid" and args.cpu_share is None:
        parser.error("--cpu-share is required in hybrid mode")
    if args.mode != "hybrid" and args.cpu_share is not None:
        parser.error("--cpu-share is only valid in hybrid mode")

    try:
        values = load_values(args.input)
        engine = DistributionEngine(DpuBackend(args.upimulator_root))
        result = engine.execute(values, args.mode, args.cpu_share)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    print("Workload: array_sum")
    print(f"Total elements: {result.total_elements}")
    print(f"Execution mode: {result.mode.upper()}")
    if result.mode == "hybrid":
        print(f"CPU share: {args.cpu_share}")
    print()
    print(f"CPU elements: {result.cpu_elements}")
    print(f"DPU elements: {result.dpu_elements}")
    print()
    print(f"CPU partial sum: {result.cpu_partial_sum}")
    print(f"DPU partial sum: {result.dpu_partial_sum}")
    if result.dpu_simulated_cycles is not None:
        print(f"DPU simulated logic cycles: {result.dpu_simulated_cycles}")
    else:
        print("DPU simulated logic cycles: not run")
    print()
    print(f"Combined result: {result.combined_result}")
    print(f"Reference result: {result.reference_result}")
    print(f"Correctness: {'PASS' if result.correct else 'FAIL'}")
    return 0 if result.correct else 1
