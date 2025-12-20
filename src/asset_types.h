#pragma once

#include "renderer/render_types.h"

#include <vector>
#include <string>

struct MeshAsset
{
    MeshID id;
    AABB aabb;
};

struct TextureAsset
{
    u32 width;
    u32 height;

    TextureID id;
};

enum EntryType
{
    INT_ENTRY = 0,
    FLOAT_ENTRY = 1,
    BOOL_ENTRY = 2,
    VEC_ENTRY = 3,
    STR_ENTRY = 4,
    STRUCT_ENTRY = 5
};

struct DataEntry
{
    std::string name;
    const EntryType type;
    const union
    {
        s32 intVal;
        f32 floatVal;
        bool boolVal;
        glm::vec3 vecVal;
        std::string stringVal;
        std::vector<DataEntry*> structVal;
    };
    DataEntry(std::string name, s32 val) : name(name), intVal(val), type(INT_ENTRY) {}
    DataEntry(std::string name, f32 val) : name(name), floatVal(val), type(FLOAT_ENTRY) {}
    DataEntry(std::string name, bool val) : name(name), boolVal(val), type(BOOL_ENTRY) {}
    DataEntry(std::string name, glm::vec3 val) : name(name), vecVal(val), type(VEC_ENTRY) {}
    DataEntry(std::string name, std::string val) : name(name), stringVal(val), type(STR_ENTRY) {}
    DataEntry(std::string name, std::vector<DataEntry*> val) : name(name), structVal(val), type(STRUCT_ENTRY) {}
    DataEntry(std::string name) : name(name), structVal(), type(STRUCT_ENTRY) {}
    ~DataEntry()
    {
        if (type == STRUCT_ENTRY)
        {
            for (DataEntry* entry : structVal)
            {
                delete entry;
            }
        }
    }
};
