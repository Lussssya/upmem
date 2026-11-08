#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$(cd "${TEST_DIR}/.." && pwd)"
ENGINE="${ENGINE_DIR}/engine"
SAMPLE_INPUT="${ENGINE_DIR}/../examples/mixed_values.txt"
SUM_PLUGIN="${ENGINE_DIR}/workloads/sum/plugin.so"
MAX_PLUGIN="${ENGINE_DIR}/workloads/max/plugin.so"
COUNT_EVEN_PLUGIN="${ENGINE_DIR}/workloads/count_even/plugin.so"
INCOMPATIBLE_PLUGIN="${TEST_DIR}/incompatible_plugin.so"
OVERLAP_PLUGIN="${TEST_DIR}/overlap_plugin.so"
JOIN_FAILURE_INJECTOR="${TEST_DIR}/join_failure_injector.so"
UPIMULATOR_ROOT="${UPIMULATOR_ROOT:-${HOME}/pim/upimulator/golang/uPIMulator}"
TEMP_DIR="$(mktemp -d)"
FAKE_ROOT="${TEMP_DIR}/fake-upimulator"
PASSED=0

cleanup() {
    rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

pass_case() {
    local name="$1"
    local expected="$2"
    shift 2
    local output

    output="$("${ENGINE}" --workload "${SUM_PLUGIN}" "$@" --upimulator-root "${UPIMULATOR_ROOT}" 2>&1)" || {
        printf 'FAIL: %s\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq "Combined result: ${expected}" <<< "${output}" || {
        printf 'FAIL: %s returned the wrong result\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq 'Correctness: PASS' <<< "${output}" || {
        printf 'FAIL: %s did not pass correctness\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    PASSED=$((PASSED + 1))
    printf 'PASS: %s\n' "${name}"
}

max_pass_case() {
    local name="$1"
    local expected="$2"
    shift 2
    local output

    output="$("${ENGINE}" --workload "${MAX_PLUGIN}" "$@" --upimulator-root "${UPIMULATOR_ROOT}" 2>&1)" || {
        printf 'FAIL: %s\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq "Workload: array_maximum" <<< "${output}" || {
        printf 'FAIL: %s did not load the maximum plugin\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq "Combined result: ${expected}" <<< "${output}" || {
        printf 'FAIL: %s returned the wrong result\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq 'Correctness: PASS' <<< "${output}" || {
        printf 'FAIL: %s did not pass correctness\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    PASSED=$((PASSED + 1))
    printf 'PASS: %s\n' "${name}"
}

count_even_pass_case() {
    local name="$1"
    local expected="$2"
    shift 2
    local output

    output="$("${ENGINE}" --workload "${COUNT_EVEN_PLUGIN}" "$@" \
        --upimulator-root "${UPIMULATOR_ROOT}" 2>&1)" || {
        printf 'FAIL: %s\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq 'Workload: count_even' <<< "${output}" || {
        printf 'FAIL: %s did not load the count-even plugin\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq "Combined result: ${expected}" <<< "${output}" || {
        printf 'FAIL: %s returned the wrong result\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    grep -Fq 'Correctness: PASS' <<< "${output}" || {
        printf 'FAIL: %s did not pass correctness\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    PASSED=$((PASSED + 1))
    printf 'PASS: %s\n' "${name}"
}

fail_case() {
    local name="$1"
    shift

    if "${ENGINE}" --workload "${SUM_PLUGIN}" "$@" --upimulator-root "${UPIMULATOR_ROOT}" >/dev/null 2>&1; then
        printf 'FAIL: %s was accepted\n' "${name}" >&2
        exit 1
    fi
    PASSED=$((PASSED + 1))
    printf 'PASS: %s rejected\n' "${name}"
}

reset_fake_backend() {
    rm -rf "${FAKE_ROOT}/bin"
    mkdir -p "${FAKE_ROOT}/bin/scalar_workload_sum_wram_1"
}

fake_failure_case() {
    local name="$1"
    local fake_case="$2"
    local expected_error="$3"
    local output
    local status

    reset_fake_backend
    set +e
    output="$(FAKE_DPU_CASE="${fake_case}" "${ENGINE}" --workload "${SUM_PLUGIN}" \
        --mode dpu --dpu-memory wram --tasklets 1 \
        --input "${SAMPLE_INPUT}" --upimulator-root "${FAKE_ROOT}" 2>&1)"
    status=$?
    set -e

    if (( status == 0 )); then
        printf 'FAIL: %s was accepted\n%s\n' "${name}" "${output}" >&2
        exit 1
    fi
    grep -Fq "${expected_error}" <<< "${output}" || {
        printf 'FAIL: %s returned the wrong error\n%s\n' "${name}" "${output}" >&2
        exit 1
    }
    PASSED=$((PASSED + 1))
    printf 'PASS: %s rejected\n' "${name}"
}

awk 'BEGIN { for (i = 0; i < 256; i++) printf "1%s", i == 255 ? "\n" : "," }' \
    > "${TEMP_DIR}/max.txt"
awk 'BEGIN { for (i = 0; i < 257; i++) printf "1%s", i == 256 ? "\n" : " " }' \
    > "${TEMP_DIR}/too_many.txt"
printf '# no values\n' > "${TEMP_DIR}/empty.txt"
printf '4294967295 1\n' > "${TEMP_DIR}/overflow.txt"
printf '0,0,0,0,0,0,0\n' > "${TEMP_DIR}/zeros.txt"
printf '7,7,7,7,7,7,7,7,7\n' > "${TEMP_DIR}/repeated.txt"
printf '1,4294967295,8,4294967295,3\n' > "${TEMP_DIR}/uint32_max.txt"
printf '2,4,6,8,10,12,14\n' > "${TEMP_DIR}/all_even.txt"
printf '1,3,5,7,9\n' > "${TEMP_DIR}/all_odd.txt"
printf '1,2\n' > "${TEMP_DIR}/overlap.txt"
mkdir -p "${FAKE_ROOT}"
cp "${TEST_DIR}/fake_runner.sh" "${FAKE_ROOT}/run_scalar_workload.sh"
cp "${TEST_DIR}/overlap_runner.sh" "${FAKE_ROOT}/overlap_runner.sh"
chmod +x "${FAKE_ROOT}/run_scalar_workload.sh"
chmod +x "${FAKE_ROOT}/overlap_runner.sh"

pass_case cpu_only 1437 \
    --mode cpu --dpu-memory mram --tasklets 24 --input "${SAMPLE_INPUT}"
pass_case dpu_wram_1 1437 \
    --mode dpu --dpu-memory wram --tasklets 1 --input "${SAMPLE_INPUT}"
pass_case dpu_mram_1 1437 \
    --mode dpu --dpu-memory mram --tasklets 1 --input "${SAMPLE_INPUT}"
pass_case dpu_wram_4 1437 \
    --mode dpu --dpu-memory wram --tasklets 4 --input "${SAMPLE_INPUT}"
pass_case dpu_mram_4 1437 \
    --mode dpu --dpu-memory mram --tasklets 4 --input "${SAMPLE_INPUT}"
pass_case hybrid_non_divisible 1437 \
    --mode hybrid --cpu-share 0.3 --dpu-memory wram --tasklets 4 --input "${SAMPLE_INPUT}"
pass_case hybrid_fewer_values_than_tasklets 1437 \
    --mode hybrid --cpu-share 0.7 --dpu-memory mram --tasklets 8 --input "${SAMPLE_INPUT}"
pass_case hybrid_empty_dpu_partition 1437 \
    --mode hybrid --cpu-share 1 --dpu-memory mram --tasklets 24 --input "${SAMPLE_INPUT}"
pass_case maximum_input_and_tasklets 256 \
    --mode dpu --dpu-memory wram --tasklets 24 --input "${TEMP_DIR}/max.txt"
pass_case maximum_mram_input_and_tasklets 256 \
    --mode dpu --dpu-memory mram --tasklets 24 --input "${TEMP_DIR}/max.txt"

max_pass_case max_cpu_only 1000 \
    --mode cpu --input "${SAMPLE_INPUT}"
max_pass_case max_dpu_wram_1 1000 \
    --mode dpu --dpu-memory wram --tasklets 1 --input "${SAMPLE_INPUT}"
max_pass_case max_dpu_mram_4 1000 \
    --mode dpu --dpu-memory mram --tasklets 4 --input "${SAMPLE_INPUT}"
max_pass_case max_hybrid_non_divisible 1000 \
    --mode hybrid --cpu-share 0.3 --dpu-memory wram --tasklets 4 --input "${SAMPLE_INPUT}"
max_pass_case max_hybrid_fewer_values_than_tasklets 1000 \
    --mode hybrid --cpu-share 0.7 --dpu-memory mram --tasklets 8 --input "${SAMPLE_INPUT}"
max_pass_case max_hybrid_empty_dpu_partition 1000 \
    --mode hybrid --cpu-share 1 --dpu-memory mram --tasklets 24 --input "${SAMPLE_INPUT}"
max_pass_case max_all_zeros 0 \
    --mode dpu --dpu-memory wram --tasklets 4 --input "${TEMP_DIR}/zeros.txt"
max_pass_case max_repeated_values 7 \
    --mode dpu --dpu-memory mram --tasklets 4 --input "${TEMP_DIR}/repeated.txt"
max_pass_case max_uint32_value 4294967295 \
    --mode hybrid --cpu-share 0.4 --dpu-memory mram --tasklets 4 --input "${TEMP_DIR}/uint32_max.txt"

count_even_pass_case count_even_cpu_only 5 \
    --mode cpu --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_dpu_wram_1 5 \
    --mode dpu --dpu-memory wram --tasklets 1 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_dpu_mram_4 5 \
    --mode dpu --dpu-memory mram --tasklets 4 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_dpu_wram_24 5 \
    --mode dpu --dpu-memory wram --tasklets 24 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_hybrid_non_divisible 5 \
    --mode hybrid --cpu-share 0.3 --dpu-memory wram --tasklets 4 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_hybrid_fewer_values_than_tasklets 5 \
    --mode hybrid --cpu-share 0.7 --dpu-memory mram --tasklets 8 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_hybrid_empty_cpu_partition 5 \
    --mode hybrid --cpu-share 0 --dpu-memory wram --tasklets 4 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_hybrid_empty_dpu_partition 5 \
    --mode hybrid --cpu-share 1 --dpu-memory mram --tasklets 24 --input "${SAMPLE_INPUT}"
count_even_pass_case count_even_all_even 7 \
    --mode dpu --dpu-memory wram --tasklets 4 --input "${TEMP_DIR}/all_even.txt"
count_even_pass_case count_even_all_odd_and_odd_length 0 \
    --mode dpu --dpu-memory mram --tasklets 4 --input "${TEMP_DIR}/all_odd.txt"

DESCRIBE_OUTPUT="$("${ENGINE}" --workload "${COUNT_EVEN_PLUGIN}" --describe-workload 2>&1)" || {
    printf 'FAIL: count-even metadata could not be inspected\n%s\n' "${DESCRIBE_OUTPUT}" >&2
    exit 1
}
if ! grep -Fq 'Name: count_even' <<< "${DESCRIBE_OUTPUT}" ||
   ! grep -Fq 'Supported elements: 1..256' <<< "${DESCRIBE_OUTPUT}" ||
   ! grep -Fq 'WRAM kernel available: yes' <<< "${DESCRIBE_OUTPUT}" ||
   ! grep -Fq 'MRAM kernel available: yes' <<< "${DESCRIBE_OUTPUT}"; then
    printf 'FAIL: count-even metadata is incomplete\n%s\n' "${DESCRIBE_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: count-even plugin metadata described\n'

fail_case invalid_memory \
    --mode dpu --dpu-memory cache --input "${SAMPLE_INPUT}"
fail_case zero_tasklets \
    --mode dpu --tasklets 0 --input "${SAMPLE_INPUT}"
fail_case too_many_tasklets \
    --mode dpu --tasklets 25 --input "${SAMPLE_INPUT}"
fail_case missing_hybrid_share \
    --mode hybrid --input "${SAMPLE_INPUT}"
fail_case invalid_share \
    --mode hybrid --cpu-share 1.1 --input "${SAMPLE_INPUT}"
fail_case empty_input \
    --mode cpu --input "${TEMP_DIR}/empty.txt"
fail_case too_many_elements \
    --mode cpu --input "${TEMP_DIR}/too_many.txt"
fail_case uint32_sum_overflow \
    --mode cpu --input "${TEMP_DIR}/overflow.txt"

if "${ENGINE}" --workload "${TEMP_DIR}/missing-plugin.so" \
    --mode cpu --input "${SAMPLE_INPUT}" >/dev/null 2>&1; then
    printf 'FAIL: missing plugin path was accepted\n' >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: missing plugin path rejected\n'

if "${ENGINE}" --workload "${INCOMPATIBLE_PLUGIN}" \
    --mode cpu --input "${SAMPLE_INPUT}" >/dev/null 2>&1; then
    printf 'FAIL: incompatible plugin ABI was accepted\n' >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: incompatible plugin ABI rejected\n'

if grep -Eiq 'array_sum|array_maximum|count_even|distributed_sum' \
    "${ENGINE_DIR}/src/engine.c" \
    "${ENGINE_DIR}/src/dpu_backend.c" \
    "${ENGINE_DIR}/src/partitioner.c"; then
    printf 'FAIL: core engine contains workload-specific operation knowledge\n' >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: core engine has no sum/max/count-even operation selection\n'

OVERLAP_DIR="${TEMP_DIR}/overlap"
mkdir -p "${OVERLAP_DIR}"
OVERLAP_OUTPUT="$(OVERLAP_TEST_DIR="${OVERLAP_DIR}" \
    "${ENGINE}" --workload "${OVERLAP_PLUGIN}" \
    --mode hybrid --cpu-share 0.5 --dpu-memory wram --tasklets 1 \
    --input "${TEMP_DIR}/overlap.txt" --upimulator-root "${FAKE_ROOT}" 2>&1)" || {
    printf 'FAIL: hybrid CPU/DPU overlap\n%s\n' "${OVERLAP_OUTPUT}" >&2
    exit 1
}
if [[ ! -e "${OVERLAP_DIR}/cpu_started" || ! -e "${OVERLAP_DIR}/dpu_started" ]] ||
   ! grep -Fq 'CPU partial result: 1' <<< "${OVERLAP_OUTPUT}" ||
   ! grep -Fq 'DPU partial result: 2' <<< "${OVERLAP_OUTPUT}" ||
   ! grep -Fq 'Combined result: 3' <<< "${OVERLAP_OUTPUT}" ||
   ! grep -Fq 'Correctness: PASS' <<< "${OVERLAP_OUTPUT}"; then
    printf 'FAIL: hybrid CPU/DPU overlap produced invalid evidence\n%s\n' \
        "${OVERLAP_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: hybrid CPU worker overlaps DPU runner\n'

rm -f "${OVERLAP_DIR}/cpu_started" "${OVERLAP_DIR}/dpu_started"
set +e
DUAL_FAILURE_OUTPUT="$(OVERLAP_TEST_DIR="${OVERLAP_DIR}" \
    OVERLAP_CPU_FAIL=1 OVERLAP_DPU_FAIL=1 \
    "${ENGINE}" --workload "${OVERLAP_PLUGIN}" \
    --mode hybrid --cpu-share 0.5 --dpu-memory wram --tasklets 1 \
    --input "${TEMP_DIR}/overlap.txt" --upimulator-root "${FAKE_ROOT}" 2>&1)"
DUAL_FAILURE_STATUS=$?
set -e
if (( DUAL_FAILURE_STATUS == 0 )) ||
   ! grep -Fq 'injected CPU backend failure' <<< "${DUAL_FAILURE_OUTPUT}" ||
   ! grep -Fq 'DPU backend also failed' <<< "${DUAL_FAILURE_OUTPUT}" ||
   ! grep -Fq 'injected DPU backend failure' <<< "${DUAL_FAILURE_OUTPUT}"; then
    printf 'FAIL: simultaneous backend diagnostics\n%s\n' \
        "${DUAL_FAILURE_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: simultaneous backend failures preserve both diagnostics\n'

rm -f "${OVERLAP_DIR}/cpu_started" "${OVERLAP_DIR}/dpu_started"
set +e
JOIN_FAILURE_OUTPUT="$(LD_PRELOAD="${JOIN_FAILURE_INJECTOR}" \
    OVERLAP_TEST_DIR="${OVERLAP_DIR}" \
    "${ENGINE}" --workload "${OVERLAP_PLUGIN}" \
    --mode hybrid --cpu-share 0.5 --dpu-memory wram --tasklets 1 \
    --input "${TEMP_DIR}/overlap.txt" --upimulator-root "${FAKE_ROOT}" 2>&1)"
JOIN_FAILURE_STATUS=$?
set -e
if (( JOIN_FAILURE_STATUS == 0 )) ||
   ! grep -Fq 'FATAL: cannot wait for CPU worker' <<< "${JOIN_FAILURE_OUTPUT}" ||
   ! grep -Fq 'terminating without unloading plugin' <<< "${JOIN_FAILURE_OUTPUT}"; then
    printf 'FAIL: pthread_join failure did not fail-stop safely\n%s\n' \
        "${JOIN_FAILURE_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: pthread_join failure uses fail-stop cleanup policy\n'

set +e
"${ENGINE}" --workload "${OVERLAP_PLUGIN}" \
    --mode dpu --dpu-memory wram --tasklets 1 \
    --input "${TEMP_DIR}/overlap.txt" --upimulator-root "${FAKE_ROOT}" \
    1>&- 2>&-
CLOSED_STDIO_STATUS=$?
set -e
if (( CLOSED_STDIO_STATUS != 0 )); then
    printf 'FAIL: posix_spawn redirection with closed stdout/stderr\n' >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: posix_spawn redirection handles closed stdout/stderr\n'

reset_fake_backend
set +e
HYBRID_FAILURE_OUTPUT="$(FAKE_DPU_CASE=incomplete_output \
    "${ENGINE}" --workload "${SUM_PLUGIN}" \
    --mode hybrid --cpu-share 0.5 --dpu-memory wram --tasklets 1 \
    --input "${SAMPLE_INPUT}" --upimulator-root "${FAKE_ROOT}" 2>&1)"
HYBRID_FAILURE_STATUS=$?
set -e
if (( HYBRID_FAILURE_STATUS == 0 )) ||
   ! grep -Fq 'captured DPU output has 3 byte values; expected exactly 4' \
       <<< "${HYBRID_FAILURE_OUTPUT}"; then
    printf 'FAIL: hybrid backend failure cleanup\n%s\n' \
        "${HYBRID_FAILURE_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: hybrid backend failure joins CPU worker and returns error\n'

reset_fake_backend
printf '157\n5\n0\n0\n' > "${FAKE_ROOT}/bin/scalar_workload_sum_wram_1/actual_output.bin"
printf 'Logic[0_0_0]_logic_cycle: 999\n' > "${FAKE_ROOT}/bin/scalar_workload_sum_wram_1/log.txt"
set +e
STALE_OUTPUT="$(FAKE_DPU_CASE=stale "${ENGINE}" --workload "${SUM_PLUGIN}" \
    --mode dpu --dpu-memory wram --tasklets 1 \
    --input "${SAMPLE_INPUT}" --upimulator-root "${FAKE_ROOT}" 2>&1)"
STALE_STATUS=$?
set -e
if (( STALE_STATUS == 0 )) || \
   ! grep -Fq 'missing freshly captured DPU output' <<< "${STALE_OUTPUT}" || \
   [[ -e "${FAKE_ROOT}/bin/scalar_workload_sum_wram_1/actual_output.bin" ]] || \
   [[ -e "${FAKE_ROOT}/bin/scalar_workload_sum_wram_1/log.txt" ]]; then
    printf 'FAIL: stale output was not rejected correctly\n%s\n' "${STALE_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: stale output rejected\n'

fake_failure_case malformed_output incomplete_output \
    'captured DPU output has 3 byte values; expected exactly 4'
fake_failure_case negative_output_byte negative_byte \
    'is not an unsigned decimal byte'
fake_failure_case oversized_output_byte oversized_byte \
    'is outside 0..255: 256'
fake_failure_case trailing_output trailing_output \
    'has unexpected trailing content'
fake_failure_case missing_cycle_statistics missing_cycles \
    'missing uPIMulator statistics'
fake_failure_case invalid_cycle_statistics invalid_cycles \
    'invalid logic-cycle value'
fake_failure_case overflowing_cycle_statistics overflowing_cycles \
    'logic-cycle value overflows uint64'
fake_failure_case trailing_cycle_statistics trailing_cycles \
    'invalid trailing content in logic-cycle value'

reset_fake_backend
set +e
INCORRECT_OUTPUT="$(FAKE_DPU_CASE=incorrect_result "${ENGINE}" --workload "${SUM_PLUGIN}" \
    --mode dpu --dpu-memory wram --tasklets 1 \
    --input "${SAMPLE_INPUT}" --upimulator-root "${FAKE_ROOT}" 2>&1)"
INCORRECT_STATUS=$?
set -e
if (( INCORRECT_STATUS != 1 )) || \
   ! grep -Fq 'Correctness: FAIL' <<< "${INCORRECT_OUTPUT}" || \
   grep -Fq 'ERROR:' <<< "${INCORRECT_OUTPUT}"; then
    printf 'FAIL: incorrect computation was not reported as correctness failure\n%s\n' \
        "${INCORRECT_OUTPUT}" >&2
    exit 1
fi
PASSED=$((PASSED + 1))
printf 'PASS: incorrect computation returned status 1\n'

printf 'All %s C-engine tests passed.\n' "${PASSED}"
