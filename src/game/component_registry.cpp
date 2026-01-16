#include <skl_math_types.h>
#include <scene.h>
#include <scene_loader.h>

#define REGISTRY
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