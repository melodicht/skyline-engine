#include <toml++/toml.hpp>
#include <iostream>


struct ComponentInfo
{
    void (*loadFunc)(Scene&, EntityID, toml::table*, int);
    size_t size;
};

std::vector<ComponentInfo> compInfos;
std::unordered_map<std::string, EntityID> entityNames;

template <typename T>
void LoadValue(char* dest, toml::node* data) {}

template <>
void LoadValue<int>(char* dest, toml::node* data)
{
    if (!data->is_integer())
    {
        std::cout << "This field must be an integer\n";
    }

    *(int*)dest = data->as_integer()->get();
}

template <>
void LoadValue<float>(char* dest, toml::node* data)
{
    if (!data->is_floating_point())
    {
        std::cout << "This field must be a floating point number\n";
    }

    *(float*)dest = data->as_floating_point()->get();
}

template <>
void LoadValue<bool>(char* dest, toml::node* data)
{
    if (!data->is_boolean())
    {
        std::cout << "This field must be a boolean\n";
    }

    *(bool*)dest = data->as_boolean()->get();
}

template <>
void LoadValue<glm::vec3>(char* dest, toml::node* data)
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

    LoadValue<float>(dest, array->get(0));
    LoadValue<float>(dest + sizeof(float), array->get(1));
    LoadValue<float>(dest + (2 * sizeof(float)), array->get(2));
}

void LoadIfPresent(char* dest, const char* name, toml::table* data, void (*loadFunc)(char*, toml::node*))
{
    if (data->contains(name))
    {
        loadFunc(dest, data->get(name));
    }
}

template <typename T>
void LoadComponent(Scene &scene, EntityID entity, toml::table* compData, int compIndex)
{
    char* comp = (char*)scene.Assign<T>(entity);
    LoadValue<T>(comp, compData);
}

template <typename T>
void AddComponent(const char *name)
{
    compName<T> = name;
    MakeComponentId(name);
    compInfos.push_back({LoadComponent<T>, sizeof(T)});
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
                compInfo.loadFunc(scene, id, val.second.as_table(), compIndex);
            }
        }


    }
    catch (const toml::parse_error& error)
    {
        std::cout << error << '\n';
    }
}
