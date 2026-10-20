import os
import shutil
import subprocess
import argparse


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--benchmark", required=True)
    parser.add_argument("--num_dpus", type=int, default=1)
    parser.add_argument("--num_tasklets", type=int, default=1)
    args = parser.parse_args()

    benchmark_dir_path = os.path.dirname(__file__)
    root_dir_path = os.path.dirname(benchmark_dir_path)

    build_dir_path = os.path.join(benchmark_dir_path, "build")

    if os.path.exists(build_dir_path):
        shutil.rmtree(build_dir_path)
    os.makedirs(build_dir_path)

    source_dir_path = os.path.join(benchmark_dir_path, args.benchmark)
    if os.path.basename(source_dir_path) != args.benchmark or not os.path.isdir(source_dir_path):
        raise ValueError(f"unknown benchmark: {args.benchmark}")

    upmem_cc = shutil.which("dpu-upmem-dpurte-clang")
    if upmem_cc is None:
        raise RuntimeError("dpu-upmem-dpurte-clang is not in PATH")

    subprocess.run(
        [
            "cmake",
            "-D",
            f"NR_DPUS={args.num_dpus}",
            "-D"
            f"NR_TASKLETS={args.num_tasklets}",
            "-D",
            f"BENCHMARK={args.benchmark}",
            "-D",
            f"UPMEM_CC={upmem_cc}",
            "-D",
            f"UPIMULATOR_ROOT={root_dir_path}",
            "-S",
            benchmark_dir_path,
            "-B",
            build_dir_path,
            "-G",
            "Ninja",
        ],
        check=True,
    )
    subprocess.run(["ninja", "-C", build_dir_path], check=True)
