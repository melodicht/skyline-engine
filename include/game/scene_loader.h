#pragma once

#include <string>
#include <vector>

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
    std::string name;
    std::string iconPath;
};

struct IconGizmo
{
    ComponentID id;
    TextureAsset* texture;
};

extern std::vector<ComponentInfo> compInfos;
extern std::unordered_map<std::string, EntityID> entityIds;
extern std::vector<IconGizmo> iconGizmos;

void RegisterComponents(Scene& scene, bool editor);

s32 LoadScene(Scene& scene, std::string name);

void SaveScene(Scene& scene, std::string name);

void SaveCurrentScene(Scene& scene);