#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_SOURCE=""
OUTPUT_TAG=""
MEMORY=""
NUM_TASKLETS=""
EXPECTED=""
VALUES_CSV=""

usage() {
    printf 'Usage: %s --kernel-source FILE --output-tag ID --memory wram|mram --tasklets 1..24 --expected UINT32 --values CSV\n' "$0"
}

while (( $# > 0 )); do
    case "$1" in
        --kernel-source|--output-tag|--memory|--tasklets|--expected|--values)
            (( $# >= 2 )) || { printf 'ERROR: %s needs a value\n' "$1" >&2; exit 2; }
            case "$1" in
                --kernel-source) KERNEL_SOURCE="$2" ;;
                --output-tag) OUTPUT_TAG="$2" ;;
                --memory) MEMORY="$2" ;;
                --tasklets) NUM_TASKLETS="$2" ;;
                --expected) EXPECTED="$2" ;;
                --values) VALUES_CSV="$2" ;;
            esac
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'ERROR: unknown argument: %s\n' "$1" >&2
            exit 2
            ;;
    esac
done

[[ -f "${KERNEL_SOURCE}" ]] || { printf 'ERROR: kernel source is not readable: %s\n' "${KERNEL_SOURCE}" >&2; exit 2; }
[[ "${OUTPUT_TAG}" =~ ^[A-Za-z0-9_-]{1,63}$ ]] || { printf 'ERROR: invalid output tag\n' >&2; exit 2; }
[[ "${MEMORY}" == "wram" || "${MEMORY}" == "mram" ]] || { printf 'ERROR: memory must be wram or mram\n' >&2; exit 2; }
[[ "${NUM_TASKLETS}" =~ ^[1-9][0-9]*$ ]] || { printf 'ERROR: tasklets must be from 1 through 24\n' >&2; exit 2; }
NUM_TASKLETS=$((10#${NUM_TASKLETS}))
(( NUM_TASKLETS <= 24 )) || { printf 'ERROR: tasklets must be from 1 through 24\n' >&2; exit 2; }
[[ "${EXPECTED}" =~ ^[0-9]+$ ]] || { printf 'ERROR: expected result must be uint32\n' >&2; exit 2; }
EXPECTED_VALUE=$((10#${EXPECTED}))
(( EXPECTED_VALUE <= 4294967295 )) || { printf 'ERROR: expected result must be uint32\n' >&2; exit 2; }
[[ -n "${VALUES_CSV}" ]] || { printf 'ERROR: values cannot be empty\n' >&2; exit 2; }

IFS=',' read -r -a VALUES <<< "${VALUES_CSV}"
(( ${#VALUES[@]} >= 1 && ${#VALUES[@]} <= 256 )) || { printf 'ERROR: provide 1 through 256 values\n' >&2; exit 2; }
for value in "${VALUES[@]}"; do
    [[ "${value}" =~ ^[0-9]+$ ]] || { printf 'ERROR: every value must be uint32\n' >&2; exit 2; }
    decimal_value=$((10#${value}))
    (( decimal_value <= 4294967295 )) || { printf 'ERROR: every value must be uint32\n' >&2; exit 2; }
done

if ! command -v go >/dev/null && [[ -x "${HOME}/.local/go/bin/go" ]]; then
    export PATH="${HOME}/.local/go/bin:${PATH}"
fi
if ! command -v dpu-upmem-dpurte-clang >/dev/null; then
    SDK_ENV="${UPMEM_SDK_DIR:-${HOME}/pim/upmem-sdk}/upmem_env.sh"
    [[ -f "${SDK_ENV}" ]] || { printf 'ERROR: source the UPMEM SDK or set UPMEM_SDK_DIR\n' >&2; exit 1; }
    source "${SDK_ENV}" simulator >/dev/null
fi

BENCHMARK="SCALAR_WORKLOAD"
BENCHMARK_TASK="${ROOT_DIR}/benchmark/${BENCHMARK}/dpu/task.c"
BIN_DIR="${ROOT_DIR}/bin/scalar_workload_${OUTPUT_TAG}_${MEMORY}_${NUM_TASKLETS}"
STATS_FILE="${BIN_DIR}/log.txt"
ACTUAL_OUTPUT_FILE="${BIN_DIR}/actual_output.bin"
MEMORY_CODE=0
[[ "${MEMORY}" == "mram" ]] && MEMORY_CODE=1

mkdir -p "${BIN_DIR}"
rm -f "${ACTUAL_OUTPUT_FILE}" "${STATS_FILE}"
cp "${KERNEL_SOURCE}" "${BENCHMARK_TASK}"

DATA_PREP_PARAMS="${MEMORY_CODE},${EXPECTED_VALUE},${#VALUES[@]},${VALUES_CSV}"
UPMEM_CC="$(command -v dpu-upmem-dpurte-clang)"

go -C "${ROOT_DIR}" build -o build/uPIMulator ./src
rm -rf "${ROOT_DIR}/benchmark/build"
cmake \
    -D "NR_DPUS=1" \
    -D "NR_TASKLETS=${NUM_TASKLETS}" \
    -D "BENCHMARK=${BENCHMARK}" \
    -D "UPMEM_CC=${UPMEM_CC}" \
    -D "UPIMULATOR_ROOT=${ROOT_DIR}" \
    -S "${ROOT_DIR}/benchmark" \
    -B "${ROOT_DIR}/benchmark/build" \
    -G Ninja >/dev/null
ninja -C "${ROOT_DIR}/benchmark/build" >/dev/null

UPIMULATOR_ACTUAL_OUTPUT="${ACTUAL_OUTPUT_FILE}" \
"${ROOT_DIR}/build/uPIMulator" \
    --root_dirpath "${ROOT_DIR}" \
    --bin_dirpath "${BIN_DIR}" \
    --benchmark "${BENCHMARK}" \
    --num_channels 1 \
    --num_ranks_per_channel 1 \
    --num_dpus_per_rank 1 \
    --num_tasklets "${NUM_TASKLETS}" \
    --num_simulation_threads 2 \
    --data_prep_params "${DATA_PREP_PARAMS}"

[[ -f "${ACTUAL_OUTPUT_FILE}" ]] || { printf 'ERROR: simulator did not capture actual output\n' >&2; exit 1; }
mapfile -t RESULT_BYTES < "${ACTUAL_OUTPUT_FILE}"
(( ${#RESULT_BYTES[@]} == 4 )) || { printf 'ERROR: expected four output bytes\n' >&2; exit 1; }
ACTUAL_RESULT=$((RESULT_BYTES[0] | (RESULT_BYTES[1] << 8) | (RESULT_BYTES[2] << 16) | (RESULT_BYTES[3] << 24)))
TOTAL_CYCLES="$(awk -F': ' '$1 == "Logic[0_0_0]_logic_cycle" { print $2; exit }' "${STATS_FILE}")"
[[ -n "${TOTAL_CYCLES}" ]] || { printf 'ERROR: logic-cycle statistic was not found\n' >&2; exit 1; }
(( ACTUAL_RESULT == EXPECTED_VALUE )) || { printf 'ERROR: actual result does not match validation result\n' >&2; exit 1; }

printf 'DPU_RESULT: workload=%s result=%s logic_cycles=%s elements=%s memory=%s tasklets=%s\n' \
    "${OUTPUT_TAG}" "${ACTUAL_RESULT}" "${TOTAL_CYCLES}" "${#VALUES[@]}" "${MEMORY^^}" "${NUM_TASKLETS}"
