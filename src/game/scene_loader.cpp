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

void RegisterComponents(Scene& scene, bool editor)
{
    for (ComponentID id = 0; id < compInfos.size(); id++)
    {
        ComponentInfo& compInfo = compInfos[id];
        stringToId[compInfo.name] = id;
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
