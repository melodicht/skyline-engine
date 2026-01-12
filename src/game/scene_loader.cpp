#include <typeindex>

#include <meta_definitions.h>
#include <scene.h>
#include <scene_loader.h>
#include <game.h>
#include <scene_view.h>
#include <entity_view.h>

file_global std::string currentSceneName = "";

std::vector<ComponentInfo> compInfos;
std::unordered_map<std::string, EntityID> entityIds;
std::vector<IconGizmo> iconGizmos;
file_global std::unordered_map<std::string, ComponentID> stringToId;

template <typename T>
const char* compName;

template <typename T>
s32 WriteFromData(T* dest, DataEntry* data) { return 0; }

template <>
s32 WriteFromData<s32>(s32* dest, DataEntry* data)
{
    if (data->type != INT_ENTRY)
    {
        printf("entry must be int but instead is %d\n", data->type);
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
        printf("entry must be float but instead is %d\n", data->type);
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
        printf("entry must be bool but instead is %d\n", data->type);
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
        printf("entry must be vec3 but instead is %d\n", data->type);
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
        printf("entry must be string but instead is %d\n", data->type);
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
        printf("entry must be string but instead is %d\n", data->type);
        return -1;
    }
    *dest = globalPlatformAPI.assetUtils.LoadMeshAsset(data->stringVal);
    return 0;
}

template <>
s32 WriteFromData<TextureAsset*>(TextureAsset** dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        printf("entry must be string but instead is %d\n", data->type);
        return -1;
    }
    if (data->stringVal != "")
    {
        *dest = globalPlatformAPI.assetUtils.LoadTextureAsset(data->stringVal);
    }
    return 0;
}

template <typename T>
DataEntry* ReadToData(T* src, std::string name)
{
    return new DataEntry(name);
}

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
    if ((*src) == nullptr)
    {
        return new DataEntry(name, std::string(""));
    }
    return new DataEntry(name, (*src)->name);
}

template <>
DataEntry* ReadToData<TextureAsset*>(TextureAsset** src, std::string name)
{
    if (*src == nullptr)
    {
        return new DataEntry(name, std::string(""));
    }
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

void RegisterComponents(Scene& scene, bool editor)
{
    for (ComponentID id = 0; id < compInfos.size(); id++)
    {
        ComponentInfo& compInfo = compInfos[id];
        scene.AddComponentPool(compInfo.size);
        if (editor && compInfo.iconPath.length() > 0)
        {
            TextureAsset* icon = globalPlatformAPI.assetUtils.LoadTextureAsset(compInfo.iconPath);
            if (icon != nullptr)
            {
                iconGizmos.push_back({id, icon});
            }
        }
    }
}

inline std::string GetCurrentSceneName()
{
    std::string result = currentSceneName;
    return result;
}

s32 LoadScene(Scene& scene, std::string name)
{
    std::string filepath = "scenes/" + name + ".toml";
    DataEntry* data = globalPlatformAPI.assetUtils.LoadDataAsset(filepath);
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
            ComponentID compIndex = stringToId[comp->name];
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
            ComponentID compIndex = stringToId[comp->name];
            ComponentInfo& compInfo = compInfos[compIndex];
            rv |= compInfo.writeFunc(scene, id, comp);
        }
    }
    delete data;
    currentSceneName = name;
    return rv;
}

DataEntry* ReadEntityToData(Scene& scene, EntityID ent)
{
    NameComponent* nameComp = scene.Get<NameComponent>(ent);
    DataEntry* data = new DataEntry(nameComp->name);
    for (ComponentID comp : EntityView(scene, ent))
    {
        ComponentInfo& compInfo = compInfos[comp];
        if (compInfo.name != NAME_COMPONENT)
        {
            data->structVal.push_back(compInfo.readFunc(scene, ent));
        }
    }
    return data;
}

void SaveScene(Scene& scene, std::string name)
{
    DataEntry* sceneData = new DataEntry("Scene");
    for (EntityID ent : SceneView<NameComponent>(scene))
    {
        sceneData->structVal.push_back(ReadEntityToData(scene, ent));
    }

    std::string filepath = "scenes/" + name + ".toml";
    globalPlatformAPI.assetUtils.WriteDataAsset(filepath, sceneData);
    delete sceneData;
}

void SaveCurrentScene(Scene& scene)
{
    SaveScene(scene, GetCurrentSceneName());
}
