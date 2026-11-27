#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
host="${script_dir}/build/metadata_dpu_search"
dataset="${script_dir}/../datasets/siftsmall"
sdk="${UPMEM_HOME:-/home/vboxuser/pim/upmem-sdk}"

export UPMEM_HOME="${sdk}"
export LD_LIBRARY_PATH="${sdk}/lib${LD_LIBRARY_PATH+:${LD_LIBRARY_PATH}}"
export UPMEM_PROFILE_BASE=backend=simulator

run_case() {
    local name=$1
    local expected_count=$2
    shift 2

    echo "===== ${name} ====="
    local output
    output=$("${host}" --dataset-dir "${dataset}" "$@")
    echo "${output}"
    grep -q "^Eligible candidates: ${expected_count}$" <<<"${output}"
    grep -q '^Correctness: PASS$' <<<"${output}"
}

run_case "restrictive filter" 18 --min-date 2027-05-01 --query-id 0 --k 5
run_case "less restrictive filter" 503 --min-date 2026-01-01 --query-id 17 --k 5
run_case "no eligible vectors" 0 --min-date 2027-05-19 --query-id 0 --k 5
run_case "fewer candidates than K" 2 --min-date 2027-05-17 --query-id 42 --k 5
run_case "different query" 18 --min-date 2027-05-01 --query-id 99 --k 3

echo "All metadata-to-DPU search tests passed."

