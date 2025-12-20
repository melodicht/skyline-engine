#include <toml++/toml.hpp>
#include <iostream>

struct NameComponent
{
    std::string name;
};

struct ComponentInfo
{
    void (*assignFunc)(Scene&, EntityID);
    s32 (*writeFunc)(Scene&, EntityID, DataEntry*);
    DataEntry* (*readFunc)(Scene&, EntityID, std::string);
    size_t size;
    const char* name;
};

std::vector<ComponentInfo> compInfos;
std::unordered_map<std::string, EntityID> entityIds;

template <typename T>
s32 WriteFromData(T* dest, DataEntry* data) { return 0; }

template <>
s32 WriteFromData<s32>(s32* dest, DataEntry* data)
{
    if (data->type != INT_ENTRY)
    {
        return -1;
    }
    *dest = data->intVal;
    return 0;
}

template <>
s32 WriteFromData<f32>(f32* dest, DataEntry* data)
{
    if (data->type != FLOAT_ENTRY)
    {
        return -1;
    }
    *dest = data->floatVal;
    return 0;
}

template <>
s32 WriteFromData<bool>(bool* dest, DataEntry* data)
{
    if (data->type != BOOL_ENTRY)
    {
        return -1;
    }

    *dest = data->boolVal;
    return 0;
}

template <>
s32 WriteFromData<glm::vec3>(glm::vec3* dest, DataEntry* data)
{
    if (data->type != VEC_ENTRY)
    {
        return -1;
    }
    *dest = data->vecVal;
    return 0;
}

template <>
s32 WriteFromData<std::string>(std::string* dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        return -1;
    }
    *dest = data->stringVal;
    return 0;
}

template <>
s32 WriteFromData<MeshAsset*>(MeshAsset** dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        return -1;
    }
    *dest = globalPlatformAPI.platformLoadMeshAsset(data->stringVal);
    return 0;
}

template <>
s32 WriteFromData<TextureAsset*>(TextureAsset** dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        return -1;
    }
    *dest = globalPlatformAPI.platformLoadTextureAsset(data->stringVal);
    return 0;
}

template <typename T>
DataEntry* ReadToData(T* src, std::string name) { return nullptr; }

template <>
DataEntry* ReadToData<s32>(s32* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<f32>(f32* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<bool>(bool* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<glm::vec3>(glm::vec3* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<std::string>(std::string* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<MeshAsset*>(MeshAsset** src, std::string name)
{
    return new DataEntry(name, (*src)->name);
}

template <>
DataEntry* ReadToData<TextureAsset*>(TextureAsset** src, std::string name)
{
    return new DataEntry(name, (*src)->name);
}

template <typename T>
s32 WriteIfPresent(T* dest, std::string name, std::vector<DataEntry*>& data)
{
    for (DataEntry* entry : data)
    {
        if (entry->name == name)
        {
            return WriteFromData<T>(dest, entry);
        }
    }
    return 0;
}

template <typename T>
void AssignComponent(Scene &scene, EntityID entity)
{
    scene.Assign<T>(entity);
}

template <typename T>
s32 WriteComponent(Scene &scene, EntityID entity, DataEntry* compData)
{
    T* comp = scene.Get<T>(entity);
    return WriteFromData<T>(comp, compData);
}

template <typename T>
DataEntry* ReadComponent(Scene &scene, EntityID entity, std::string name)
{
    T* comp = scene.Get<T>(entity);
    return ReadToData<T>(comp, name);
}

template <typename T>
void AddComponent(const char *name)
{
    compName<T> = name;
    MakeComponentId(name);
    compInfos.push_back({AssignComponent<T>, WriteComponent<T>, ReadComponent<T>, sizeof(T), name});
}

void RegisterComponents(Scene& scene)
{
    for (ComponentInfo& compInfo : compInfos)
    {
        scene.AddComponentPool(compInfo.size);
    }
}

s32 LoadScene(Scene& scene, std::string name)
{
    std::string filepath = "scenes/" + name + ".toml";
    DataEntry* data = globalPlatformAPI.platformLoadDataAsset(filepath, name);
    if (data->type != STRUCT_ENTRY)
    {
        return -1;
    }
    for (DataEntry* entity : data->structVal)
    {
        if (entity->type != STRUCT_ENTRY)
        {
            return -1;
        }
        EntityID id = scene.NewEntity();
        entityIds[entity->name] = id;

        NameComponent* nameComp = scene.Assign<NameComponent>(id);
        nameComp->name = entity->name;

        for (DataEntry* comp : entity->structVal)
        {
            if (!stringToId.contains(comp->name))
            {
                return -1;
            }
            s32 compIndex = stringToId[comp->name];
            ComponentInfo& compInfo = compInfos[compIndex];
            compInfo.assignFunc(scene, id);
        }
    }
    s32 rv = 0;
    for (DataEntry* entity : data->structVal)
    {
        EntityID id = entityIds[entity->name];
        for (DataEntry* comp : entity->structVal)
        {
            int compIndex = stringToId[comp->name];
            ComponentInfo& compInfo = compInfos[compIndex];
            rv |= compInfo.writeFunc(scene, id, comp);
        }
    }
    delete data;
    return rv;
}
