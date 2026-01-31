#pragma once

// TODO(marvin): Scene dependency is only really needed for the system vtable. Perhaps that should be defined in its own file?
#include <scene.h>
#include <typeindex>

// Responsible for accumulating compile time information about a
// system into a run-time construct.

// NOTE(marvin): Design mirrors component_registry.h and
// scene_loader.h. Unlike the two, the system registry doesn't deal
// with the scene.

std::unordered_map<std::type_index, SystemVTable*>& SystemTypeToVTable();

template <typename T>
void RegisterSystem(SystemVTable *vtable)
{
    SystemTypeToVTable()[std::type_index(typeid(T))] = vtable;
}

