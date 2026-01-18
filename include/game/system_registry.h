#pragma once

// Responsible for accumulating compile time information about a
// system into a run-time construct.

// NOTE(marvin): Design mirrors component_registry.h and
// scene_loader.h. Unlike the two, the system registry doesn't deal
// with the scene.

struct SystemInfo
{
    void (*updateSystemVTableFunc)();
};

std::vector<SystemInfo> &SystemInfos();

void RegisterSystems();

template <typename T>
void UpdateSystemVTable()
{
    T::UpdateVTable();
}

template <typename T>
void AddSystem()
{
    SystemInfos().push_back({UpdateSystemVTable<T>});
}

// NOTE(marvin): The (...) assigned to a variable which we ignore, for
// the purpose executing the function on static init.
#define SYSTEM(type) [[maybe_unused]] static int ignore_add##type = (AddSystem<type>(), 0);
