#include <toml++/toml.hpp>
#include <iostream>

struct EditorMetadata
{
    std::string name;
};

struct ComponentInfo
{
    void (*assignFunc)(Scene&, EntityID);
    s32 (*writeFunc)(Scene&, EntityID, DataEntry*);
    size_t size;
    const char* name;
};

std::vector<ComponentInfo> compInfos;
std::unordered_map<std::string, EntityID> entityNames;

template <typename T>
void LoadValue(T* dest, toml::node* data) {}

template <>
void LoadValue<int>(int* dest, toml::node* data)
{
    if (!data->is_integer())
    {
        std::cout << "This field must be an integer\n";
    }

    *dest = data->as_integer()->get();
}

template <>
void LoadValue<float>(float* dest, toml::node* data)
{
    if (!data->is_floating_point())
    {
        std::cout << "This field must be a floating point number\n";
    }

    *dest = data->as_floating_point()->get();
}

template <>
void LoadValue<bool>(bool* dest, toml::node* data)
{
    if (!data->is_boolean())
    {
        std::cout << "This field must be a boolean\n";
    }

    *dest = data->as_boolean()->get();
}

template <>
void LoadValue<glm::vec3>(glm::vec3* dest, toml::node* data)
{
    if (!data->is_array())
    {
        std::cout << "This field must be an array\n";
    }

    toml::array* array = data->as_array();

    if (array->size() != 3)
    {
        std::cout << "This field must have a length of 3\n";
    }

    LoadValue<float>(&dest->x, array->get(0));
    LoadValue<float>(&dest->y, array->get(1));
    LoadValue<float>(&dest->z, array->get(2));
}

template <>
void LoadValue<MeshAsset*>(MeshAsset** dest, toml::node* data)
{
    if (!data->is_string())
    {
        std::cout << "This field must be an string\n";
    }

    *dest = globalPlatformAPI.platformLoadMeshAsset(data->as_string()->get());
}

template <>
void LoadValue<TextureAsset*>(TextureAsset** dest, toml::node* data)
{
    if (!data->is_string())
    {
        std::cout << "This field must be an string\n";
    }

    *dest = globalPlatformAPI.platformLoadTextureAsset(data->as_string()->get());
}

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
void LoadIfPresent(T* dest, const char* name, toml::table* data)
{
    if (data->contains(name))
    {
        LoadValue<T>(dest, data->get(name));
    }
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
void LoadComponent(Scene &scene, EntityID entity, toml::table* compData)
{
    T* comp = scene.Get<T>(entity);
    LoadValue<T>(comp, compData);
}

template <typename T>
s32 WriteComponent(Scene &scene, EntityID entity, DataEntry* compData)
{
    T* comp = scene.Get<T>(entity);
    return WriteFromData<T>(comp, compData);
}

template <typename T>
void AddComponent(const char *name)
{
    compName<T> = name;
    MakeComponentId(name);
    compInfos.push_back({AssignComponent<T>, WriteComponent<T>, sizeof(T), name});
}

void RegisterComponents(Scene& scene)
{
    for (ComponentInfo& compInfo : compInfos)
    {
        scene.AddComponentPool(compInfo.size);
    }
}

int LoadScene(Scene& scene, std::string name, bool editor)
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
        entityNames[entity->name] = id;
        if (editor)
        {
            EditorMetadata* metadata = scene.Assign<EditorMetadata>(id);
            metadata->name = entity->name;
        }
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
        EntityID id = entityNames[entity->name];
        for (DataEntry* comp : entity->structVal)
        {
            int compIndex = stringToId[comp->name];
            ComponentInfo& compInfo = compInfos[compIndex];
            rv |= compInfo.writeFunc(scene, id, comp);
        }
    }
    return rv;
}
