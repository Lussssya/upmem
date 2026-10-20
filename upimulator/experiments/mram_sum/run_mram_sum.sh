#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${ROOT_DIR}/bin/mram_sum"
STATS_FILE="${BIN_DIR}/log.txt"
ARRAY_LENGTH="${1:-256}"

if [[ ! "${ARRAY_LENGTH}" =~ ^[1-9][0-9]*$ ]] || (( ARRAY_LENGTH > 256 )); then
    printf 'ERROR: elements must be an integer from 1 through 256\n' >&2
    exit 2
fi

if (( $# > 1 )); then
    printf 'ERROR: usage: %s [elements]\n' "$0" >&2
    exit 2
fi

EXPECTED_SUM="$((ARRAY_LENGTH * (ARRAY_LENGTH + 1) / 2))"

if ! command -v go >/dev/null && [[ -x "${HOME}/.local/go/bin/go" ]]; then
    export PATH="${HOME}/.local/go/bin:${PATH}"
fi
if ! command -v dpu-upmem-dpurte-clang >/dev/null; then
    SDK_ENV="${UPMEM_SDK_DIR:-${HOME}/pim/upmem-sdk}/upmem_env.sh"
    [[ -f "${SDK_ENV}" ]] || { printf 'ERROR: source the UPMEM SDK or set UPMEM_SDK_DIR\n' >&2; exit 1; }
    source "${SDK_ENV}" simulator >/dev/null
fi

mkdir -p "${BIN_DIR}"
printf 'Running uPIMulator mram_sum: elements=%s block_elements=16\n' "${ARRAY_LENGTH}"

go -C "${ROOT_DIR}" build -o build/uPIMulator ./src
python3 "${ROOT_DIR}/benchmark/build.py" \
    --benchmark MRAM_SUM --num_dpus 1 --num_tasklets 1

"${ROOT_DIR}/build/uPIMulator" \
    --root_dirpath "${ROOT_DIR}" \
    --bin_dirpath "${BIN_DIR}" \
    --benchmark MRAM_SUM \
    --num_channels 1 \
    --num_ranks_per_channel 1 \
    --num_dpus_per_rank 1 \
    --num_tasklets 1 \
    --num_simulation_threads 2 \
    --data_prep_params "${ARRAY_LENGTH}"

TOTAL_CYCLES="$(awk -F': ' '$1 == "Logic[0_0_0]_logic_cycle" { print $2; exit }' "${STATS_FILE}")"
if [[ -z "${TOTAL_CYCLES}" ]]; then
    printf 'ERROR: logic-cycle statistic was not found in %s\n' "${STATS_FILE}" >&2
    exit 1
fi

printf 'PASS: final sum is %s.\n' "${EXPECTED_SUM}"
printf 'SUMMARY: elements=%s memory=MRAM final_sum=%s total_simulated_cycles=%s\n' \
    "${ARRAY_LENGTH}" "${EXPECTED_SUM}" "${TOTAL_CYCLES}"
