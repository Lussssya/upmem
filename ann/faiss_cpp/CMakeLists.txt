cmake_minimum_required(VERSION 3.20)
project(faiss_ivf_flat_siftsmall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FAISS_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/.deps/install/faiss-1.16.0"
    CACHE PATH "CPU-only FAISS installation prefix")
set(OPENBLAS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/openblas"
    CACHE PATH "Locally extracted OpenBLAS prefix")
set(GFORTRAN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/gfortran"
    CACHE PATH "Locally extracted libgfortran prefix")

list(PREPEND CMAKE_PREFIX_PATH "${FAISS_INSTALL_PREFIX}")
find_package(faiss CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(ivf_flat_siftsmall
    src/main.cpp
    src/vecs_io.cpp
)
target_include_directories(ivf_flat_siftsmall PRIVATE include)
target_include_directories(ivf_flat_siftsmall SYSTEM PRIVATE
    "${FAISS_INSTALL_PREFIX}/include")
target_link_libraries(ivf_flat_siftsmall PRIVATE faiss_avx2 OpenMP::OpenMP_CXX)
target_compile_options(ivf_flat_siftsmall PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
)
target_link_options(ivf_flat_siftsmall PRIVATE -Wl,--disable-new-dtags)

set(OPENBLAS_LIBRARY_DIR
    "${OPENBLAS_ROOT}/usr/lib/x86_64-linux-gnu/openblas-pthread")
set(GFORTRAN_LIBRARY_DIR "${GFORTRAN_ROOT}/usr/lib/x86_64-linux-gnu")
set(GFORTRAN_LIBRARY "${GFORTRAN_LIBRARY_DIR}/libgfortran.so.5")
if(NOT EXISTS "${GFORTRAN_LIBRARY}")
    message(FATAL_ERROR "Missing local libgfortran runtime: ${GFORTRAN_LIBRARY}")
endif()
target_link_libraries(ivf_flat_siftsmall PRIVATE "${GFORTRAN_LIBRARY}")
set_target_properties(ivf_flat_siftsmall PROPERTIES
    BUILD_RPATH
        "${FAISS_INSTALL_PREFIX}/lib;${OPENBLAS_LIBRARY_DIR};${GFORTRAN_LIBRARY_DIR}"
)

