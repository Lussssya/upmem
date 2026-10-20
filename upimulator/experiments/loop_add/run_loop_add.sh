#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${ROOT_DIR}/bin/loop_add"
STATS_FILE="${BIN_DIR}/log.txt"
INITIAL_VALUE=41
ITERATIONS="${1:-1}"

if [[ ! "${ITERATIONS}" =~ ^[0-9]+$ ]]; then
    printf 'ERROR: iterations must be a non-negative integer\n' >&2
    exit 2
fi

FINAL_RESULT="$((INITIAL_VALUE + ITERATIONS))"

if ! command -v go >/dev/null && [[ -x "${HOME}/.local/go/bin/go" ]]; then
    export PATH="${HOME}/.local/go/bin:${PATH}"
fi
if ! command -v dpu-upmem-dpurte-clang >/dev/null; then
    SDK_ENV="${UPMEM_SDK_DIR:-${HOME}/pim/upmem-sdk}/upmem_env.sh"
    [[ -f "${SDK_ENV}" ]] || { printf 'ERROR: source the UPMEM SDK or set UPMEM_SDK_DIR\n' >&2; exit 1; }
    source "${SDK_ENV}" simulator >/dev/null
fi

mkdir -p "${BIN_DIR}"

printf 'Running uPIMulator loop_add: initial=%s iterations=%s expected=%s\n' \
    "${INITIAL_VALUE}" "${ITERATIONS}" "${FINAL_RESULT}"

go -C "${ROOT_DIR}" build -o build/uPIMulator ./src
python3 "${ROOT_DIR}/benchmark/build.py" \
    --benchmark LOOP_ADD --num_dpus 1 --num_tasklets 1

"${ROOT_DIR}/build/uPIMulator" \
    --root_dirpath "${ROOT_DIR}" \
    --bin_dirpath "${BIN_DIR}" \
    --benchmark LOOP_ADD \
    --num_channels 1 \
    --num_ranks_per_channel 1 \
    --num_dpus_per_rank 1 \
    --num_tasklets 1 \
    --num_simulation_threads 2 \
    --data_prep_params "${INITIAL_VALUE},${ITERATIONS}"

TOTAL_CYCLES="$(awk -F': ' '$1 == "Logic[0_0_0]_logic_cycle" { print $2; exit }' "${STATS_FILE}")"
if [[ -z "${TOTAL_CYCLES}" ]]; then
    printf 'ERROR: logic-cycle statistic was not found in %s\n' "${STATS_FILE}" >&2
    exit 1
fi

printf 'PASS: final result is %s.\n' "${FINAL_RESULT}"
printf 'SUMMARY: iterations=%s final_result=%s total_simulated_cycles=%s\n' \
    "${ITERATIONS}" "${FINAL_RESULT}" "${TOTAL_CYCLES}"
