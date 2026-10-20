cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED UPMEM_CC)
    find_program(UPMEM_CC dpu-upmem-dpurte-clang REQUIRED)
endif()
set(CMAKE_C_COMPILER "${UPMEM_CC}" CACHE FILEPATH "UPMEM DPU compiler" FORCE)
set(CMAKE_C_COMPILER_WORKS TRUE CACHE BOOL "UPMEM compiler is prevalidated" FORCE)
project(sdk C)

add_subdirectory(misc)
add_subdirectory(stdlib)
add_subdirectory(syslib)
