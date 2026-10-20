# Increment test

Run the default `41 -> 42` test:

```bash
./run_increment.sh
```

Or pass another integer:

```bash
./run_increment.sh 99
```

Files: `run_increment.sh` is the installed runner, `task.c` is the DPU kernel,
and `host.go` prepares the input and expected result. The two CMake files build
the kernel. Run the command from the uPIMulator root after using `install.sh`.
