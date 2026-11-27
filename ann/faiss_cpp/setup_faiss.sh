#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
deps_dir="${script_dir}/.deps"
openblas_version="0.3.26+ds-1ubuntu0.1"
gfortran_version="14.2.0-4ubuntu2~24.04.1"
faiss_tag="v1.16.0"
faiss_commit="6ccfd5f9be2283ebc037564bb45c6ba63b71e96a"
jobs=${BUILD_JOBS:-2}

for command in apt-get cmake dpkg-deb git ninja; do
    if ! command -v "${command}" >/dev/null 2>&1; then
        echo "Required command is unavailable: ${command}" >&2
        exit 1
    fi
done

mkdir -p "${deps_dir}/packages" "${deps_dir}/openblas" \
    "${deps_dir}/gfortran" "${deps_dir}/src" "${deps_dir}/build" \
    "${deps_dir}/install"

if [[ ! -f "${deps_dir}/openblas/usr/lib/x86_64-linux-gnu/openblas-pthread/libopenblas.so" ]]; then
    (
        cd "${deps_dir}/packages"
        apt-get download \
            "libopenblas-pthread-dev=${openblas_version}" \
            "libopenblas0-pthread=${openblas_version}"
        for package in ./libopenblas-pthread-dev_*.deb \
            ./libopenblas0-pthread_*.deb; do
            dpkg-deb -x "${package}" "${deps_dir}/openblas"
        done
    )
fi

if [[ ! -f "${deps_dir}/gfortran/usr/lib/x86_64-linux-gnu/libgfortran.so.5" ]]; then
    (
        cd "${deps_dir}/packages"
        apt-get download "libgfortran5=${gfortran_version}"
        for package in ./libgfortran5_*.deb; do
            dpkg-deb -x "${package}" "${deps_dir}/gfortran"
        done
    )
fi

if [[ ! -d "${deps_dir}/src/faiss/.git" ]]; then
    git clone --depth 1 --branch "${faiss_tag}" \
        https://github.com/facebookresearch/faiss.git \
        "${deps_dir}/src/faiss"
fi

actual_commit=$(git -C "${deps_dir}/src/faiss" rev-parse HEAD)
if [[ "${actual_commit}" != "${faiss_commit}" ]]; then
    echo "Unexpected FAISS source commit: ${actual_commit}" >&2
    echo "Expected ${faiss_commit} (${faiss_tag})." >&2
    exit 1
fi

openblas_library="${deps_dir}/openblas/usr/lib/x86_64-linux-gnu/openblas-pthread/libopenblas.so"
install_prefix="${deps_dir}/install/faiss-1.16.0"

cmake -S "${deps_dir}/src/faiss" -B "${deps_dir}/build/faiss" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_prefix}" \
    -DBLAS_LIBRARIES="${openblas_library}" \
    -DLAPACK_LIBRARIES="${openblas_library}" \
    -DFAISS_ENABLE_GPU=OFF \
    -DFAISS_ENABLE_ROCM=OFF \
    -DFAISS_ENABLE_CUVS=OFF \
    -DFAISS_ENABLE_MKL=OFF \
    -DFAISS_ENABLE_PYTHON=OFF \
    -DFAISS_ENABLE_C_API=OFF \
    -DFAISS_ENABLE_EXTRAS=OFF \
    -DFAISS_ENABLE_SVS=OFF \
    -DFAISS_OPT_LEVEL=avx2 \
    -DBUILD_TESTING=OFF \
    -DBUILD_SHARED_LIBS=ON

cmake --build "${deps_dir}/build/faiss" --parallel "${jobs}"
cmake --install "${deps_dir}/build/faiss"

echo "FAISS ${faiss_tag} installed at ${install_prefix}"

