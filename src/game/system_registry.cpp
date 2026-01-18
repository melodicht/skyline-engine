#include <meta_definitions.h>
#include <system_registry.h>

std::unordered_map<std::type_index, SystemVTable*>& SystemTypeToVTable()
{
    file_global std::unordered_map<std::type_index, SystemVTable*> systemTypeToVTable;
    return systemTypeToVTable;
}


