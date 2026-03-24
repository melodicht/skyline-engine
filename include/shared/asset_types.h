#pragma once

#include <string>

#include <glm/glm.hpp>

#include <meta_definitions.h>
#include <render_types.h>

struct MeshAsset
{
    std::string name;
    MeshID id;
    AABB aabb;
};

struct TextureAsset
{
    std::string name;
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
        s64 intVal;
        f64 floatVal;
        bool boolVal;
        glm::vec3 vecVal;
        std::string stringVal;
        std::vector<DataEntry*> structVal;
    };
    DataEntry(std::string name, s64 val) : name(name), type(INT_ENTRY), intVal(val) {}
    DataEntry(std::string name, f64 val) : name(name), type(FLOAT_ENTRY), floatVal(val) {}
    DataEntry(std::string name, bool val) : name(name), type(BOOL_ENTRY), boolVal(val) {}
    DataEntry(std::string name, glm::vec3 val) : name(name), type(VEC_ENTRY), vecVal(val) {}
    DataEntry(std::string name, std::string val) : name(name), type(STR_ENTRY), stringVal(val) {}
    DataEntry(std::string name, std::vector<DataEntry*> val) : name(name), type(STRUCT_ENTRY), structVal(val) {}
    DataEntry(std::string name) : name(name), type(STRUCT_ENTRY), structVal() {}
    DataEntry(DataEntry& other) : name(other.name), type(other.type)
    {
        switch (other.type)
        {
        case INT_ENTRY:
            intVal = other.intVal;
            break;
        case FLOAT_ENTRY:
            floatVal = other.floatVal;
            break;
        case BOOL_ENTRY:
            boolVal = other.boolVal;
            break;
        case VEC_ENTRY:
            vecVal = other.vecVal;
            break;
        case STR_ENTRY:
            stringVal = other.stringVal;
        case STRUCT_ENTRY:
            structVal = other.structVal;
            break;
        }
    }
    ~DataEntry()
    {
        if (type == STR_ENTRY)
        {
            stringVal.~basic_string();
        }
        if (type == STRUCT_ENTRY)
        {
            for (DataEntry* entry : structVal)
            {
                delete entry;
            }
            structVal.~vector();
        }
    }
};

struct ActorField
{
    std::string name;
    std::string linkEntity;
    std::string linkComponent;
    std::string linkField;
};

struct ActorAsset
{
    std::string name;
    std::vector<ActorField> fields;
    std::vector<DataEntry*> entities;
    ~ActorAsset()
    {
        for (DataEntry* entry : entities)
        {
            delete entry;
        }
    }
};
