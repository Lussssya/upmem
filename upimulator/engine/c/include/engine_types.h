#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENGINE_MAX_ELEMENTS 256
#define ENGINE_PATH_CAPACITY 4096
#define ENGINE_WORKLOAD_NAME_CAPACITY 128

typedef enum {
    MODE_CPU,
    MODE_DPU,
    MODE_HYBRID
} ExecutionMode;

typedef enum {
    DPU_MEMORY_WRAM,
    DPU_MEMORY_MRAM
} DpuMemory;

typedef struct {
    ExecutionMode mode;
    bool mode_set;
    long double cpu_share;
    bool cpu_share_set;
    DpuMemory dpu_memory;
    unsigned int tasklets;
    char workload_path[ENGINE_PATH_CAPACITY];
    char input_path[ENGINE_PATH_CAPACITY];
    char upimulator_root[ENGINE_PATH_CAPACITY];
} EngineConfig;

typedef struct {
    uint32_t values[ENGINE_MAX_ELEMENTS];
    size_t count;
} WorkloadInput;

typedef struct {
    size_t cpu_offset;
    size_t cpu_count;
    size_t dpu_offset;
    size_t dpu_count;
} WorkPartition;

typedef struct {
    uint32_t partial_result;
    bool ran;
    uint64_t simulated_cycles;
} BackendResult;

typedef struct {
    char workload_name[ENGINE_WORKLOAD_NAME_CAPACITY];
    size_t total_elements;
    size_t cpu_elements;
    size_t dpu_elements;
    uint32_t cpu_partial_result;
    uint32_t dpu_partial_result;
    uint32_t combined_result;
    uint32_t reference_result;
    bool correct;
    bool dpu_ran;
    uint64_t dpu_simulated_cycles;
} ExecutionResult;

#endif
