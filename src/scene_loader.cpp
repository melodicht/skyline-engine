#include <toml++/toml.hpp>
#include <iostream>


struct ComponentInfo
{
    void (*assignFunc)(Scene&, EntityID);
    void (*loadFunc)(Scene&, EntityID, toml::table*);
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
void LoadIfPresent(T* dest, const char* name, toml::table* data)
{
    if (data->contains(name))
    {
        LoadValue<T>(dest, data->get(name));
    }
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
void AddComponent(const char *name)
{
    compName<T> = name;
    MakeComponentId(name);
    compInfos.push_back({AssignComponent<T>, LoadComponent<T>, sizeof(T)});
}

void RegisterComponents(Scene& scene)
{
    for (ComponentInfo& compInfo : compInfos)
    {
        scene.AddComponentPool(compInfo.size);
    }
}

void LoadScene(Scene& scene, const char* filename)
{
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(filename);

        for (auto entity : tbl)
        {
            toml::table* table = entity.second.as_table();
            EntityID id = scene.NewEntity();
            entityNames[entity.first.data()] = id;

            for (auto val : *table)
            {
                if (!stringToId.contains(val.first.data()))
                {
                    std::cout << "Invalid Component: " << val.first.data() <<"\n";
                    exit(0);
                }

                int compIndex = stringToId[val.first.data()];
                ComponentInfo& compInfo = compInfos[compIndex];
                compInfo.assignFunc(scene, id);
            }
        }

        for (auto entity : tbl)
        {
            toml::table* table = entity.second.as_table();
            EntityID id = entityNames[entity.first.data()];

            for (auto val : *table)
            {
                int compIndex = stringToId[val.first.data()];
                ComponentInfo& compInfo = compInfos[compIndex];
                compInfo.loadFunc(scene, id, val.second.as_table());
            }
        }
    }
    catch (const toml::parse_error& error)
    {
        std::cout << error << '\n';
    }
}
