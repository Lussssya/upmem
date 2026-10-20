cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED UPMEM_CC)
    find_program(UPMEM_CC dpu-upmem-dpurte-clang REQUIRED)
endif()
set(CMAKE_C_COMPILER "${UPMEM_CC}" CACHE FILEPATH "UPMEM DPU compiler" FORCE)
set(CMAKE_C_COMPILER_WORKS TRUE CACHE BOOL "UPMEM compiler is prevalidated" FORCE)
project(benchmark C)

if(NOT DEFINED BENCHMARK)
    message(FATAL_ERROR "BENCHMARK must be provided")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BENCHMARK}/CMakeLists.txt")
    message(FATAL_ERROR "Unknown benchmark: ${BENCHMARK}")
endif()

add_subdirectory("${BENCHMARK}")
