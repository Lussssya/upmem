#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${ROOT_DIR}/bin/increment"
STATS_FILE="${BIN_DIR}/log.txt"
INPUT_VALUE="${1:-41}"

if ! command -v go >/dev/null && [[ -x "${HOME}/.local/go/bin/go" ]]; then
    export PATH="${HOME}/.local/go/bin:${PATH}"
fi
if ! command -v dpu-upmem-dpurte-clang >/dev/null; then
    SDK_ENV="${UPMEM_SDK_DIR:-${HOME}/pim/upmem-sdk}/upmem_env.sh"
    [[ -f "${SDK_ENV}" ]] || { printf 'ERROR: source the UPMEM SDK or set UPMEM_SDK_DIR\n' >&2; exit 1; }
    source "${SDK_ENV}" simulator >/dev/null
fi

mkdir -p "${BIN_DIR}"

printf 'Running uPIMulator increment example: %s -> %s\n' \
    "${INPUT_VALUE}" "$((INPUT_VALUE + 1))"

go -C "${ROOT_DIR}" build -o build/uPIMulator ./src
python3 "${ROOT_DIR}/benchmark/build.py" \
    --benchmark INCREMENT --num_dpus 1 --num_tasklets 1

"${ROOT_DIR}/build/uPIMulator" \
    --root_dirpath "${ROOT_DIR}" \
    --bin_dirpath "${BIN_DIR}" \
    --benchmark INCREMENT \
    --num_channels 1 \
    --num_ranks_per_channel 1 \
    --num_dpus_per_rank 1 \
    --num_tasklets 1 \
    --num_simulation_threads 2 \
    --data_prep_params "${INPUT_VALUE}"

TOTAL_CYCLES="$(awk -F': ' '$1 == "Logic[0_0_0]_logic_cycle" { print $2; exit }' "${STATS_FILE}")"
if [[ -z "${TOTAL_CYCLES}" ]]; then
    printf 'ERROR: logic-cycle statistic was not found in %s\n' "${STATS_FILE}" >&2
    exit 1
fi

printf 'PASS: uPIMulator produced %s from %s.\n' "$((INPUT_VALUE + 1))" "${INPUT_VALUE}"
printf 'SUMMARY: total simulated cycles: %s (source: Logic[0_0_0]_logic_cycle)\n' \
    "${TOTAL_CYCLES}"
