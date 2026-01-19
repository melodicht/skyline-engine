#pragma once

#include <string>
#include <vector>
#include <typeindex>

struct NameComponent
{
    std::string name;
};

#define NAME_COMPONENT "NameComponent"

struct Scene;
struct DataEntry;
struct TextureAsset;

struct ComponentInfo
{
    void (*assignFunc)(Scene&, EntityID);
    void (*removeFunc)(Scene&, EntityID);
    s32 (*writeFunc)(Scene&, EntityID, DataEntry*);
    DataEntry* (*readFunc)(Scene&, EntityID);
    size_t size;
    std::type_index type;
    std::string name;
    std::string iconPath;
};

struct IconGizmo
{
    ComponentID id;
    TextureAsset* texture;
};

extern std::unordered_map<std::string, EntityID> entityIds;
extern std::vector<IconGizmo> iconGizmos;

std::vector<ComponentInfo>& CompInfos();

void RegisterComponents(bool editor);

void CreateComponentPools(Scene& scene);

s32 LoadMap(Scene& scene, std::string name);

void SaveMap(Scene& scene, std::string name);

void SaveCurrentMap(Scene& scene);
