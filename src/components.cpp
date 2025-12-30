#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "renderer/render_types.h"
#include "math/skl_math_utils.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Character/CharacterVirtual.h>

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

// Define the game's components here

SERIALIZE(Transform3D, position, rotation, scale)
COMPONENT(Transform3D)
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


struct MeshComponent
{
    MeshAsset* mesh;
    TextureAsset* texture;
    glm::vec3 color = glm::vec3{1.0f};
    bool dirty = true;
};
SERIALIZE(MeshComponent, mesh, texture, color)
COMPONENT(MeshComponent)


struct PlayerCharacter
{
    JPH::CharacterVirtual* characterVirtual = nullptr;
    f32 moveSpeed = 5.0f;
};
SERIALIZE(PlayerCharacter, moveSpeed)
COMPONENT(PlayerCharacter)


struct StaticBox
{
    glm::vec3 volume;
    bool initialized = false;
};
SERIALIZE(StaticBox, volume)
COMPONENT(StaticBox)

struct CameraComponent
{
    f32 fov = 90;
    f32 nearPlane = 0.01;
    f32 farPlane = 1000;
};
SERIALIZE(CameraComponent, fov, nearPlane, farPlane)
COMPONENT(CameraComponent, "gizmos/camera")


struct FlyingMovement
{
    f32 moveSpeed = 5;
    f32 turnSpeed = 0.1;
};
SERIALIZE(FlyingMovement, moveSpeed, turnSpeed)
COMPONENT(FlyingMovement)

struct HorizontalLook
{
    f32 turnSpeed = 0.1f;
};
SERIALIZE(HorizontalLook, turnSpeed)
COMPONENT(HorizontalLook)

struct VerticalLook
{
    f32 turnSpeed = 0.1;
};
SERIALIZE(VerticalLook, turnSpeed)
COMPONENT(VerticalLook)


struct Plane
{
    f32 width = 1;
    f32 length = 1;
};
SERIALIZE(Plane, width, length)
COMPONENT(Plane)


struct DirLight
{
    glm::vec3 diffuse = glm::vec3{1};
    glm::vec3 specular = glm::vec3{1};
    LightID lightID = -1;
};
SERIALIZE(DirLight, diffuse, specular)
COMPONENT(DirLight, "gizmos/dir_light")


struct SpotLight
{
    glm::vec3 diffuse = glm::vec3{1};
    glm::vec3 specular = glm::vec3{1};
    LightID lightID = -1;

    f32 innerCone = 30;
    f32 outerCone = 45;
    f32 range = 100;
};
SERIALIZE(SpotLight, diffuse, specular, innerCone, outerCone, range)
COMPONENT(SpotLight, "gizmos/spot_light")


struct PointLight
{
    glm::vec3 diffuse = glm::vec3{1};
    glm::vec3 specular = glm::vec3{1};
    LightID lightID = -1;

    f32 radius;
    f32 falloff;
};
SERIALIZE(PointLight, diffuse, specular, radius, falloff)
COMPONENT(PointLight, "gizmos/point_light")


COMPONENT(NameComponent)
