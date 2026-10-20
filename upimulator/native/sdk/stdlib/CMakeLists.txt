set(CMAKE_C_FLAGS "-O3 -S -DNR_TASKLETS=${NR_TASKLETS}")

include_directories("${CMAKE_SOURCE_DIR}/misc")
include_directories("${CMAKE_SOURCE_DIR}/stdlib")
include_directories("${CMAKE_SOURCE_DIR}/syslib")

file(GLOB_RECURSE SRCS *.c)

add_library(stdlib ${SRCS})
