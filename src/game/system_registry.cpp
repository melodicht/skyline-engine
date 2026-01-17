#include <meta_definitions.h>
#include <system_registry.h>

std::vector<SystemInfo> &SystemInfos()
{
    file_global std::vector<SystemInfo> systemInfos;
    return systemInfos;
}

// What it means for a system to be registered is for its internal
// vtable to correspond to the memory addresses of its functions in
// the game module DLL.
void RegisterSystems()
{
    std::vector<SystemInfo> &systemInfos = SystemInfos();

    for (u32 index = 0; index < systemInfos.size(); ++index)
    {
        SystemInfo &systemInfo = systemInfos[index];
        systemInfo.updateSystemVTableFunc();
    }
}
