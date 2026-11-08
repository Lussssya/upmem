#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_TAG=""
MEMORY=""
NUM_TASKLETS=""
EXPECTED=""

while (( $# > 0 )); do
    case "$1" in
        --output-tag) OUTPUT_TAG="$2"; shift 2 ;;
        --memory) MEMORY="$2"; shift 2 ;;
        --tasklets) NUM_TASKLETS="$2"; shift 2 ;;
        --expected) EXPECTED="$2"; shift 2 ;;
        --kernel-source|--values) shift 2 ;;
        *) printf 'Unexpected overlap-runner argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

[[ -n "${OUTPUT_TAG}" && -n "${MEMORY}" ]]
[[ -n "${NUM_TASKLETS}" && "${EXPECTED}" =~ ^[0-9]+$ ]]

: >&1
: >&2

if [[ -n "${OVERLAP_TEST_DIR:-}" ]]; then
    : > "${OVERLAP_TEST_DIR}/dpu_started"
    for ((attempt = 0; attempt < 500; attempt++)); do
        [[ -e "${OVERLAP_TEST_DIR}/cpu_started" ]] && break
        sleep 0.01
    done
    [[ -e "${OVERLAP_TEST_DIR}/cpu_started" ]] || {
        printf 'CPU worker did not overlap DPU runner\n' >&2
        exit 1
    }
fi

if [[ -n "${OVERLAP_DPU_FAIL:-}" ]]; then
    printf 'injected DPU backend failure\n' >&2
    exit 1
fi

BIN_DIR="${ROOT_DIR}/bin/scalar_workload_${OUTPUT_TAG}_${MEMORY}_${NUM_TASKLETS}"
mkdir -p "${BIN_DIR}"
printf '%s\n' \
    "$((EXPECTED & 255))" \
    "$(((EXPECTED >> 8) & 255))" \
    "$(((EXPECTED >> 16) & 255))" \
    "$(((EXPECTED >> 24) & 255))" \
    > "${BIN_DIR}/actual_output.bin"
printf 'Logic[0_0_0]_logic_cycle: 123\n' > "${BIN_DIR}/log.txt"
