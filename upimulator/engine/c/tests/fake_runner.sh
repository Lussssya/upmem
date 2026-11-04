#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_TAG=""
MEMORY=""
NUM_TASKLETS=""

while (( $# > 0 )); do
    case "$1" in
        --output-tag) OUTPUT_TAG="$2"; shift 2 ;;
        --memory) MEMORY="$2"; shift 2 ;;
        --tasklets) NUM_TASKLETS="$2"; shift 2 ;;
        --kernel-source|--expected|--values) shift 2 ;;
        *) printf 'Unexpected fake-runner argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

[[ -n "${OUTPUT_TAG}" && -n "${MEMORY}" && -n "${NUM_TASKLETS}" ]]
BIN_DIR="${ROOT_DIR}/bin/scalar_workload_${OUTPUT_TAG}_${MEMORY}_${NUM_TASKLETS}"
OUTPUT_FILE="${BIN_DIR}/actual_output.bin"
STATISTICS_FILE="${BIN_DIR}/log.txt"

mkdir -p "${BIN_DIR}"

write_correct_output() {
    # 1437 == 0x0000059d in little-endian byte order.
    printf '157\n5\n0\n0\n' > "${OUTPUT_FILE}"
}

write_valid_cycles() {
    printf 'Logic[0_0_0]_logic_cycle: 123\n' > "${STATISTICS_FILE}"
}

case "${FAKE_DPU_CASE:-}" in
    stale)
        # Intentionally generate nothing after the backend removes old files.
        ;;
    incomplete_output)
        printf '157\n5\n0\n' > "${OUTPUT_FILE}"
        write_valid_cycles
        ;;
    negative_byte)
        printf '%s\n' '-1' '5' '0' '0' > "${OUTPUT_FILE}"
        write_valid_cycles
        ;;
    oversized_byte)
        printf '256\n5\n0\n0\n' > "${OUTPUT_FILE}"
        write_valid_cycles
        ;;
    trailing_output)
        printf '157\n5\n0\n0\nunexpected\n' > "${OUTPUT_FILE}"
        write_valid_cycles
        ;;
    missing_cycles)
        write_correct_output
        ;;
    invalid_cycles)
        write_correct_output
        printf 'Logic[0_0_0]_logic_cycle: not-a-number\n' > "${STATISTICS_FILE}"
        ;;
    overflowing_cycles)
        write_correct_output
        printf 'Logic[0_0_0]_logic_cycle: 18446744073709551616\n' > "${STATISTICS_FILE}"
        ;;
    trailing_cycles)
        write_correct_output
        printf 'Logic[0_0_0]_logic_cycle: 123 cycles\n' > "${STATISTICS_FILE}"
        ;;
    incorrect_result)
        printf '0\n0\n0\n0\n' > "${OUTPUT_FILE}"
        write_valid_cycles
        ;;
    *)
        printf 'Unknown FAKE_DPU_CASE: %s\n' "${FAKE_DPU_CASE:-}" >&2
        exit 2
        ;;
esac
