# CPU–DPU distribution engine

The active engine is the generic compiled C implementation in [`c/`](c/). It
loads workload implementations from shared libraries and supports WRAM/MRAM
selection and 1–24 tasklets. Array sum and array maximum are example plugins.
The original Python MVP below remains as a reference; normal C-engine execution
does not require Python.

This is the first correctness-focused distribution engine for the capstone. It
supports one workload (`array_sum`) and three execution modes:

- `cpu`: sum every element natively in Python;
- `dpu`: send every element to one simulated DPU;
- `hybrid`: sum a prefix on the CPU, a suffix on the simulated DPU, and combine
  both partial results.

The first `floor(element_count × cpu_share)` elements go to the CPU. The rest
go to the DPU. Execution is sequential in this MVP.

## Input format and limits

Input files contain decimal integers separated by whitespace or commas. `#`
starts a comment. Values are arbitrary within these limits:

- 1 through 256 total elements;
- each value is a `uint32` (`0..4294967295`);
- the complete sum must fit in `uint32`.

The 256-element limit comes from the DPU kernel's fixed WRAM array. It applies
to the whole engine in this first version so every mode accepts the same input.

## Install the DPU backend

From a clone of this repository on the Linux simulator machine:

```bash
./upimulator/install.sh ~/pim/upimulator/golang/uPIMulator
```

## Run

From the repository root:

```bash
python3 -m upimulator.engine --mode cpu \
  --input upimulator/engine/examples/mixed_values.txt

python3 -m upimulator.engine --mode dpu \
  --input upimulator/engine/examples/mixed_values.txt

python3 -m upimulator.engine --mode hybrid --cpu-share 0.4 \
  --input upimulator/engine/examples/mixed_values.txt
```

Override the installed simulator location with `--upimulator-root` or the
`UPIMULATOR_ROOT` environment variable.

## Actual DPU result

The simulator's original output path only compares actual bytes with expected
bytes. The install overlay adds an opt-in capture hook controlled by
`UPIMULATOR_ACTUAL_OUTPUT`. The DPU runner enables it and stores the actual
four-byte result in:

```text
<uPIMulator>/bin/distributed_sum/actual_output.bin
```

`DpuBackend` decodes this file. It does not use the expected-output file as the
DPU partial result.

## Unit tests

```bash
python3 -m unittest discover -s upimulator/engine/tests -v
```
