#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MEMORY="wram"
NUM_TASKLETS=1
VALUES_CSV=""

usage() {
    printf 'Usage: %s [--memory wram|mram] [--tasklets 1..24] value1,value2,...\n' "$0"
}

while (( $# > 0 )); do
    case "$1" in
        --memory)
            (( $# >= 2 )) || { printf 'ERROR: --memory needs a value\n' >&2; exit 2; }
            MEMORY="$2"
            shift 2
            ;;
        --tasklets)
            (( $# >= 2 )) || { printf 'ERROR: --tasklets needs a value\n' >&2; exit 2; }
            NUM_TASKLETS="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --*)
            printf 'ERROR: unknown option: %s\n' "$1" >&2
            exit 2
            ;;
        *)
            [[ -z "${VALUES_CSV}" ]] || { printf 'ERROR: only one value list is accepted\n' >&2; exit 2; }
            VALUES_CSV="$1"
            shift
            ;;
    esac
done

if [[ "${MEMORY}" != "wram" && "${MEMORY}" != "mram" ]]; then
    printf 'ERROR: memory must be wram or mram\n' >&2
    exit 2
fi
if [[ ! "${NUM_TASKLETS}" =~ ^[1-9][0-9]*$ ]]; then
    printf 'ERROR: tasklet count must be from 1 through 24\n' >&2
    exit 2
fi
NUM_TASKLETS=$((10#${NUM_TASKLETS}))
if (( NUM_TASKLETS > 24 )); then
    printf 'ERROR: tasklet count must be from 1 through 24\n' >&2
    exit 2
fi
if [[ -z "${VALUES_CSV}" ]]; then
    usage >&2
    exit 2
fi

if [[ "${MEMORY}" == "wram" ]]; then
    BENCHMARK="DISTRIBUTED_SUM"
else
    BENCHMARK="DISTRIBUTED_SUM_MRAM"
fi

if [[ "${MEMORY}" == "wram" && "${NUM_TASKLETS}" == "1" ]]; then
    BIN_DIR="${ROOT_DIR}/bin/distributed_sum"
else
    BIN_DIR="${ROOT_DIR}/bin/distributed_sum_${MEMORY}_${NUM_TASKLETS}"
fi
STATS_FILE="${BIN_DIR}/log.txt"
ACTUAL_OUTPUT_FILE="${BIN_DIR}/actual_output.bin"

IFS=',' read -r -a VALUES <<< "${VALUES_CSV}"
if (( ${#VALUES[@]} < 1 || ${#VALUES[@]} > 256 )); then
    printf 'ERROR: provide from 1 through 256 values\n' >&2
    exit 2
fi

EXPECTED_SUM=0
for value in "${VALUES[@]}"; do
    if [[ ! "${value}" =~ ^[0-9]+$ ]]; then
        printf 'ERROR: every value must be a uint32 integer\n' >&2
        exit 2
    fi

    decimal_value=$((10#${value}))
    if (( decimal_value > 4294967295 )); then
        printf 'ERROR: every value must be a uint32 integer\n' >&2
        exit 2
    fi

    EXPECTED_SUM=$((EXPECTED_SUM + decimal_value))
    if (( EXPECTED_SUM > 4294967295 )); then
        printf 'ERROR: sum must fit in uint32\n' >&2
        exit 2
    fi
done

if ! command -v go >/dev/null && [[ -x "${HOME}/.local/go/bin/go" ]]; then
    export PATH="${HOME}/.local/go/bin:${PATH}"
fi
if ! command -v dpu-upmem-dpurte-clang >/dev/null; then
    SDK_ENV="${UPMEM_SDK_DIR:-${HOME}/pim/upmem-sdk}/upmem_env.sh"
    [[ -f "${SDK_ENV}" ]] || { printf 'ERROR: source the UPMEM SDK or set UPMEM_SDK_DIR\n' >&2; exit 1; }
    source "${SDK_ENV}" simulator >/dev/null
fi

mkdir -p "${BIN_DIR}"
rm -f "${ACTUAL_OUTPUT_FILE}" "${STATS_FILE}"

DATA_PREP_PARAMS="${#VALUES[@]},${VALUES_CSV}"
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

if [[ ! -f "${ACTUAL_OUTPUT_FILE}" ]]; then
    printf 'ERROR: uPIMulator did not capture the actual DPU output\n' >&2
    exit 1
fi

mapfile -t RESULT_BYTES < "${ACTUAL_OUTPUT_FILE}"
if (( ${#RESULT_BYTES[@]} != 4 )); then
    printf 'ERROR: expected a four-byte DPU result, found %s bytes\n' "${#RESULT_BYTES[@]}" >&2
    exit 1
fi

ACTUAL_SUM=$((
    RESULT_BYTES[0] |
    (RESULT_BYTES[1] << 8) |
    (RESULT_BYTES[2] << 16) |
    (RESULT_BYTES[3] << 24)
))

TOTAL_CYCLES="$(awk -F': ' '$1 == "Logic[0_0_0]_logic_cycle" { print $2; exit }' "${STATS_FILE}")"
if [[ -z "${TOTAL_CYCLES}" ]]; then
    printf 'ERROR: logic-cycle statistic was not found in %s\n' "${STATS_FILE}" >&2
    exit 1
fi
if (( ACTUAL_SUM != EXPECTED_SUM )); then
    printf 'ERROR: DPU result %s does not match expected sum %s\n' \
        "${ACTUAL_SUM}" "${EXPECTED_SUM}" >&2
    exit 1
fi

printf 'DPU_RESULT: sum=%s logic_cycles=%s elements=%s memory=%s tasklets=%s\n' \
    "${ACTUAL_SUM}" "${TOTAL_CYCLES}" "${#VALUES[@]}" "${MEMORY^^}" "${NUM_TASKLETS}"
