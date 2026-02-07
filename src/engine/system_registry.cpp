#include <meta_definitions.h>
#include <system_registry.h>

std::unordered_map<std::type_index, SystemIndex>& SystemTypeToIndex()
{
    file_global std::unordered_map<std::type_index, SystemIndex> systemTypeToIndex;
    return systemTypeToIndex;
}

// NOTE(marvin): Reserving 0 for problematic case.
SystemIndex globalNextSystemIndex = 1;

SystemVTable** SystemIndexToVTable()
{
    // NOTE(marvin): Times 2 because fixed and variable timestep systems each get MAX_SYSTEMS.
    file_global SystemVTable* systemIndexToVTable[MAX_SYSTEMS * 2 + 1];
    return systemIndexToVTable;
}


