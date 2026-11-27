# FAISS C++ IVF-Flat on SIFT-small

This is the native C++ CPU baseline corresponding to
`../faiss_cpu/ivf_flat.py`. It loads the official TEXMEX SIFT-small files,
trains `faiss::IndexIVFFlat`, searches the base vectors, and validates the
results against both TEXMEX ground truth and exact `faiss::IndexFlatL2`.

It does not implement metadata filtering or DPU execution. The input, search,
and evaluation code are kept separate enough to add metadata and selectors in
a later experiment without changing the vector-file reader.

## FAISS dependency

No system FAISS development package is required. The setup script downloads
the official FAISS `v1.16.0` source at commit
`6ccfd5f9be2283ebc037564bb45c6ba63b71e96a` and builds a CPU-only shared
library with OpenMP and AVX2. CUDA, ROCm, Python bindings, tests, and extras are
disabled. OpenBLAS 0.3.26 is extracted locally from the Ubuntu 24.04 packages;
its `libgfortran5` runtime is extracted in the same way. The script does not
require `sudo` or install a system package.

All downloaded and generated dependencies stay under `.deps/`, separate from
the UPMEM SDK and uPIMulator, and are ignored by Git.

```bash
cd /home/vboxuser/pim/upmem-tests/ann/faiss_cpp
./setup_faiss.sh
```

The setup requires network access the first time and the Ubuntu 24.04
`apt-get`, `dpkg-deb`, CMake, Ninja, Git, and a C++ compiler.

## Build and run

```bash
cd /home/vboxuser/pim/upmem-tests/ann/faiss_cpp
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ivf_flat_siftsmall --dataset-dir ../datasets/siftsmall
```

Defaults match the Python baseline:

- `nlist=100`
- `nprobe=10`
- `k=100`
- squared L2 distance

The parameters can be changed explicitly:

```bash
./build/ivf_flat_siftsmall \
    --dataset-dir ../datasets/siftsmall \
    --nlist 100 --nprobe 10 --k 100
```

The program reports overlap Recall@100 and TEXMEX Recall@1, Recall@10, and
Recall@100. It also fails unless exact FAISS matches the supplied ground truth
and IVF with `nprobe=nlist` reproduces exact IDs and distances.

## Source layout

- `include/vecs_io.hpp` and `src/vecs_io.cpp`: strict little-endian TEXMEX
  `.fvecs` and `.ivecs` readers with malformed-file checks.
- `src/main.cpp`: dataset validation, IVF/exact search, recall calculations,
  and verification.
- `CMakeLists.txt`: native executable build and local FAISS/OpenBLAS linkage.
- `setup_faiss.sh`: reproducible isolated FAISS dependency build.

