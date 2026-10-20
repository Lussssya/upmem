#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UPIMULATOR_ROOT="${1:-}"
EXPECTED_COMMIT="870d916334e9ff0b190f555f951a9ec3c4257781"

if [[ -z "${UPIMULATOR_ROOT}" ]]; then
    printf 'Usage: %s /path/to/uPIMulator/golang/uPIMulator\n' "$0" >&2
    exit 2
fi

UPIMULATOR_ROOT="$(cd "${UPIMULATOR_ROOT}" && pwd)"
if [[ ! -f "${UPIMULATOR_ROOT}/go.mod" || ! -d "${UPIMULATOR_ROOT}/benchmark" ]]; then
    printf 'ERROR: %s is not the Go uPIMulator root\n' "${UPIMULATOR_ROOT}" >&2
    exit 1
fi

REPOSITORY_ROOT="$(cd "${UPIMULATOR_ROOT}/../.." && pwd)"
if git -C "${REPOSITORY_ROOT}" rev-parse --git-dir >/dev/null 2>&1; then
    ACTUAL_COMMIT="$(git -C "${REPOSITORY_ROOT}" rev-parse HEAD)"
    if [[ "${ACTUAL_COMMIT}" != "${EXPECTED_COMMIT}" ]]; then
        printf 'WARNING: tested with uPIMulator %s, found %s\n' \
            "${EXPECTED_COMMIT}" "${ACTUAL_COMMIT}" >&2
    fi
fi

cp -R "${PACKAGE_DIR}/native/." "${UPIMULATOR_ROOT}/"
cp "${PACKAGE_DIR}/common/simple_sum_common.go" \
    "${UPIMULATOR_ROOT}/src/assembler/prim/simple_sum_common.go"

python3 - "${UPIMULATOR_ROOT}" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
fixes = {
    root / "src/linker/parser/parser.go": (
        "\t\t\t\t\ttoken_type == lexer.NC5 ||\n"
        "\t\t\t\t\ttoken_type == lexer.NC5 ||\n",
        "\t\t\t\t\ttoken_type == lexer.NC5 ||\n",
    ),
    root / "src/misc/command_line_parser.go": (
        'fmt.Sprintf("option (%s) is already added to the parser")',
        'fmt.Sprintf("option (%s) is already added to the parser", option)',
    ),
}

for path, (old, new) in fixes.items():
    text = path.read_text()
    if new in text and old not in text:
        continue
    if text.count(old) != 1:
        raise SystemExit(f"expected one compatibility pattern in {path}")
    path.write_text(text.replace(old, new, 1))
PY

install_experiment() {
    local directory="$1"
    local benchmark="$2"
    local go_file="$3"
    local source_dir="${PACKAGE_DIR}/experiments/${directory}"
    local benchmark_dir="${UPIMULATOR_ROOT}/benchmark/${benchmark}"

    mkdir -p "${benchmark_dir}/dpu"
    cp "${source_dir}/CMakeLists.txt" "${benchmark_dir}/CMakeLists.txt"
    cp "${source_dir}/dpu.CMakeLists.txt" "${benchmark_dir}/dpu/CMakeLists.txt"
    cp "${source_dir}/task.c" "${benchmark_dir}/dpu/task.c"
    cp "${source_dir}/host.go" "${UPIMULATOR_ROOT}/src/assembler/prim/${go_file}.go"
    cp "${source_dir}/run_${directory}.sh" "${UPIMULATOR_ROOT}/run_${directory}.sh"
    chmod +x "${UPIMULATOR_ROOT}/run_${directory}.sh"
}

install_experiment increment INCREMENT increment
install_experiment loop_add LOOP_ADD loop_add
install_experiment wram_sum WRAM_SUM wram_sum
install_experiment mram_sum MRAM_SUM mram_sum
install_experiment tasklet_sum TASKLET_SUM tasklet_sum

REGISTRY="${UPIMULATOR_ROOT}/src/assembler/assembler.go"
register_benchmark() {
    local entry="$1"
    local temporary

    if grep -Fq "${entry}" "${REGISTRY}"; then
        return
    fi

    temporary="$(mktemp)"
    awk -v entry="${entry}" '
        !inserted && /this\.assemblables\["BS"\]/ {
            print "\t" entry
            inserted = 1
        }
        { print }
        END { if (!inserted) exit 1 }
    ' "${REGISTRY}" > "${temporary}"
    mv "${temporary}" "${REGISTRY}"
}

register_benchmark 'this.assemblables["INCREMENT"] = new(prim.Increment)'
register_benchmark 'this.assemblables["LOOP_ADD"] = new(prim.LoopAdd)'
register_benchmark 'this.assemblables["WRAM_SUM"] = new(prim.WramSum)'
register_benchmark 'this.assemblables["MRAM_SUM"] = new(prim.MramSum)'
register_benchmark 'this.assemblables["TASKLET_SUM"] = new(prim.TaskletSum)'

if command -v gofmt >/dev/null; then
    gofmt -w "${REGISTRY}" "${UPIMULATOR_ROOT}/src/assembler/prim/"{increment,loop_add,wram_sum,mram_sum,tasklet_sum,simple_sum_common}.go
fi

printf 'Installed uPIMulator capstone experiments in %s\n' "${UPIMULATOR_ROOT}"
printf 'Example: cd %s && ./run_loop_add.sh 100\n' "${UPIMULATOR_ROOT}"
