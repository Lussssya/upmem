#!/usr/bin/env bash
set -euo pipefail

ENGINE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="${ENGINE_DIR}/engine"
INPUT="${ENGINE_DIR}/demo_input.txt"
UPIMULATOR_ROOT="${UPIMULATOR_ROOT:-${HOME}/pim/upimulator/golang/uPIMulator}"

required_artifacts=(
    "${ENGINE}"
    "${ENGINE_DIR}/workloads/sum/plugin.so"
    "${ENGINE_DIR}/workloads/max/plugin.so"
    "${ENGINE_DIR}/workloads/count_even/plugin.so"
)

for artifact in "${required_artifacts[@]}"; do
    if [[ ! -e "${artifact}" ]]; then
        printf 'Required build artifact is missing: %s\n' "${artifact}" >&2
        printf 'Run: make -C %s\n' "${ENGINE_DIR}" >&2
        exit 1
    fi
done

run_demo() {
    local title="$1"
    local plugin="$2"
    local expected="$3"
    shift 3
    local output

    printf '\n===== %s =====\n' "${title}"
    if ! output="$("${ENGINE}" \
        --workload "${ENGINE_DIR}/workloads/${plugin}/plugin.so" \
        --input "${INPUT}" \
        --upimulator-root "${UPIMULATOR_ROOT}" \
        "$@" 2>&1)"; then
        printf '%s\n' "${output}" >&2
        printf 'DEMO FAILED: %s\n' "${title}" >&2
        exit 1
    fi
    printf '%s\n' "${output}"
    if ! grep -Fq "Combined result: ${expected}" <<< "${output}" ||
       ! grep -Fq 'Correctness: PASS' <<< "${output}"; then
        printf 'DEMO FAILED: %s returned an unexpected result\n' "${title}" >&2
        exit 1
    fi
}

printf 'Input: [3, 4, 7, 8, 10]\n'
printf 'Known results: sum=32, maximum=10, count_even=3\n'

run_demo 'SUM / CPU-only' sum 32 \
    --mode cpu
run_demo 'SUM / DPU-only / WRAM / 4 tasklets' sum 32 \
    --mode dpu --dpu-memory wram --tasklets 4
run_demo 'MAX / hybrid 40% CPU / MRAM / 8 tasklets' max 10 \
    --mode hybrid --cpu-share 0.4 --dpu-memory mram --tasklets 8
run_demo 'COUNT_EVEN / CPU-only' count_even 3 \
    --mode cpu
run_demo 'COUNT_EVEN / DPU-only / MRAM / 4 tasklets' count_even 3 \
    --mode dpu --dpu-memory mram --tasklets 4
run_demo 'COUNT_EVEN / hybrid 40% CPU / WRAM / 2 tasklets' count_even 3 \
    --mode hybrid --cpu-share 0.4 --dpu-memory wram --tasklets 2

printf '\nDemo completed: all 6 configurations passed.\n'
