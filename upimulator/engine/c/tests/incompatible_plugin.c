#include "workload_plugin.h"

static const WorkloadPluginV1 incompatible = {
    .abi_version = 999,
    .struct_size = sizeof(WorkloadPluginV1),
};

const WorkloadPluginV1 *upmem_workload_plugin_v1(void)
{
    return &incompatible;
}
