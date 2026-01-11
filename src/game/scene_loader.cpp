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

template <typename T>
void AssignComponent(Scene &scene, EntityID entity)
{
    scene.Assign<T>(entity);
}

template <typename T>
void RemoveComponent(Scene &scene, EntityID entity)
{
    scene.Remove<T>(entity);
}

template <typename T>
s32 WriteComponent(Scene &scene, EntityID entity, DataEntry* compData)
{
    T* comp = scene.Get<T>(entity);
    if (comp == nullptr)
    {
        printf("entity must have component but doesn't\n");
        return -1;
    }
    return WriteFromData<T>(comp, compData);
}

template <typename T>
DataEntry* ReadComponent(Scene &scene, EntityID entity)
{
    T* comp = scene.Get<T>(entity);
    if (comp == nullptr)
    {
        return nullptr;
    }
    return ReadToData<T>(comp, compName<T>);
}

template <typename T>
void AddComponent(const char *name)
{
    compName<T> = name;
    ComponentID id = MakeComponentId(name);
    stringToId[name] = id;
    typeToId[std::type_index(typeid(T))] = id;
    compInfos.push_back({AssignComponent<T>, RemoveComponent<T>, WriteComponent<T>, ReadComponent<T>, sizeof(T), name});
}

template <typename T>
void AddComponent(const char *name, const char *icon)
{
    compName<T> = name;
    ComponentID id = MakeComponentId(name);
    stringToId[name] = id;
    typeToId[std::type_index(typeid(T))] = id;
    compInfos.push_back({AssignComponent<T>, RemoveComponent<T>, WriteComponent<T>, ReadComponent<T>, sizeof(T), name, icon});
}

#define PARENS ()

#define EXPAND(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_FIELDS(f, type, ...) \
    __VA_OPT__(EXPAND(_FOR_FIELDS(f, type, __VA_ARGS__)))
#define _FOR_FIELDS(f, type, a1, ...) \
    f(type, a1) \
    __VA_OPT__(__FOR_FIELDS PARENS (f, type, __VA_ARGS__))
#define __FOR_FIELDS() _FOR_FIELDS


#define WRITE_FIELD(type, field) \
    rv |= WriteIfPresent<decltype(type::field)>(&dest->field, #field, data->structVal);

#define READ_FIELD(type, field) \
    data->structVal.push_back(ReadToData<decltype(type::field)>(&src->field, #field));

#define SERIALIZE(name, ...) \
    template<> \
    s32 WriteFromData<name>(name* dest, DataEntry* data) \
    { \
        if (data->type != STRUCT_ENTRY) \
        { \
            printf("entry must be struct but instead is %d\n", data->type); \
            return -1; \
        } \
        s32 rv = 0; \
        FOR_FIELDS(WRITE_FIELD, name, __VA_ARGS__) \
        return rv; \
    } \
    template <> \
    DataEntry* ReadToData<name>(name* src, std::string name) \
    { \
        DataEntry* data = new DataEntry(name); \
        FOR_FIELDS(READ_FIELD, name, __VA_ARGS__) \
        return data; \
    }

#define COMPONENT(type, ...) [[maybe_unused]] static int add##type = (AddComponent<type>(#type __VA_OPT__(,) __VA_ARGS__), 0);

#include <components.h>

template <>
s32 WriteComponent<Transform3D>(Scene &scene, EntityID entity, DataEntry* compData)
{
    Transform3D* comp = scene.Get<Transform3D>(entity);
    s32 rv = WriteFromData<Transform3D>(comp, compData);
    for (DataEntry* entry : compData->structVal)
    {
        if (entry->name == "parent")
        {
            if (entry->type != STR_ENTRY)
            {
                printf("entry must be string but instead is %d\n", entry->type);
                return -1;
            }
            std::string parentName = entry->stringVal;
            if (!entityIds.contains(parentName))
            {
                comp->SetParent(nullptr);
                return rv;
            }
            EntityID parent = entityIds[parentName];
            Transform3D* parentTransform = scene.Get<Transform3D>(parent);
            if (parentTransform == nullptr)
            {
                comp->SetParent(nullptr);
                return rv;
            }
            comp->SetParent(parentTransform);
        }
    }

    comp->MarkDirty();
    return rv;
}

template <>
DataEntry* ReadComponent<Transform3D>(Scene &scene, EntityID entity)
{
    Transform3D* comp = scene.Get<Transform3D>(entity);
    DataEntry* data = ReadToData<Transform3D>(comp, "Transform3D");
    Transform3D* parent = comp->GetParent();
    if (parent != nullptr)
    {
        EntityID parentEnt = scene.GetOwner<Transform3D>(parent);
        NameComponent* nameComp = scene.Get<NameComponent>(parentEnt);
        data->structVal.push_back(new DataEntry("parent", nameComp->name));
    }
    else
    {
        data->structVal.push_back(new DataEntry("parent", std::string{""}));
    }
    return data;
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
