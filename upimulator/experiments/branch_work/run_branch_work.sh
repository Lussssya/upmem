#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${ROOT_DIR}/bin/branch_work"
STATS_FILE="${BIN_DIR}/log.txt"
SELECTOR="${1:-0}"

if [[ ! "${SELECTOR}" =~ ^[0-9]+$ ]]; then
    printf 'ERROR: selector must be a non-negative integer\n' >&2
    exit 2
fi

if [[ "${SELECTOR}" == "0" ]]; then
    PATH_NAME=short
    WORK_ITERATIONS=10
else
    PATH_NAME=long
    WORK_ITERATIONS=1000
fi

if ! command -v go >/dev/null && [[ -x "${HOME}/.local/go/bin/go" ]]; then
    export PATH="${HOME}/.local/go/bin:${PATH}"
fi
if ! command -v dpu-upmem-dpurte-clang >/dev/null; then
    SDK_ENV="${UPMEM_SDK_DIR:-${HOME}/pim/upmem-sdk}/upmem_env.sh"
    [[ -f "${SDK_ENV}" ]] || { printf 'ERROR: source the UPMEM SDK or set UPMEM_SDK_DIR\n' >&2; exit 1; }
    source "${SDK_ENV}" simulator >/dev/null
fi

mkdir -p "${BIN_DIR}"

printf 'Running uPIMulator branch_work: selector=%s path=%s work=%s\n' \
    "${SELECTOR}" "${PATH_NAME}" "${WORK_ITERATIONS}"

go -C "${ROOT_DIR}" build -o build/uPIMulator ./src
python3 "${ROOT_DIR}/benchmark/build.py" \
    --benchmark BRANCH_WORK --num_dpus 1 --num_tasklets 1

"${ROOT_DIR}/build/uPIMulator" \
    --root_dirpath "${ROOT_DIR}" \
    --bin_dirpath "${BIN_DIR}" \
    --benchmark BRANCH_WORK \
    --num_channels 1 \
    --num_ranks_per_channel 1 \
    --num_dpus_per_rank 1 \
    --num_tasklets 1 \
    --num_simulation_threads 2 \
    --data_prep_params "${SELECTOR}"

TOTAL_CYCLES="$(awk -F': ' '$1 == "Logic[0_0_0]_logic_cycle" { print $2; exit }' "${STATS_FILE}")"
if [[ -z "${TOTAL_CYCLES}" ]]; then
    printf 'ERROR: logic-cycle statistic was not found in %s\n' "${STATS_FILE}" >&2
    exit 1
fi

printf 'PASS: final result is %s.\n' "${WORK_ITERATIONS}"
printf 'SUMMARY: selector=%s path=%s final_result=%s total_simulated_cycles=%s\n' \
    "${SELECTOR}" "${PATH_NAME}" "${WORK_ITERATIONS}" "${TOTAL_CYCLES}"
