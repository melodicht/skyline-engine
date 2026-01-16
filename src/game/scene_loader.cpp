#include <typeindex>

#include <meta_definitions.h>
#include <scene.h>
#include <scene_loader.h>
#include <game.h>
#include <scene_view.h>
#include <entity_view.h>

file_global std::string currentSceneName = "";

std::unordered_map<std::string, EntityID> entityIds;
std::vector<IconGizmo> iconGizmos;
file_global std::unordered_map<std::string, ComponentID> stringToId;

std::vector<ComponentInfo>& CompInfos()
{
    file_global std::vector<ComponentInfo> compInfos;
    return compInfos;
}

void RegisterComponents(bool editor)
{
    for (ComponentID id = 0; id < CompInfos().size(); id++)
    {
        ComponentInfo& compInfo = CompInfos()[id];
        stringToId[compInfo.name] = id;
        typeToId[compInfo.type] = id;
        if (editor && compInfo.iconPath.length() > 0)
        {
            TextureAsset* icon = assetUtils.LoadTextureAsset(compInfo.iconPath);
            if (icon != nullptr)
            {
                iconGizmos.push_back({id, icon});
            }
        }
    }
}

void CreateComponentPools(Scene& scene)
{
    for (ComponentID id = 0; id < CompInfos().size(); id++)
    {
        ComponentInfo& compInfo = CompInfos()[id];
        scene.AddComponentPool(compInfo.size);
    }
}

inline std::string GetCurrentSceneName()
{
    std::string result = currentSceneName;
    return result;
}

s32 LoadScene(Scene& scene, std::string name)
{
    std::string filepath = "scenes/" + name;
    DataEntry* data = assetUtils.LoadDataAsset(filepath);
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
            ComponentInfo& compInfo = CompInfos()[compIndex];
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
            ComponentInfo& compInfo = CompInfos()[compIndex];
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
        ComponentInfo& compInfo = CompInfos()[comp];
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

    std::string filepath = "scenes/" + name;
    assetUtils.WriteDataAsset(filepath, sceneData);
    delete sceneData;
}

void SaveCurrentScene(Scene& scene)
{
    SaveScene(scene, GetCurrentSceneName());
}
