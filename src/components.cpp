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


#define ADD_FIELD(type, field) \
    LoadIfPresent<decltype(type::field)>(&dest->field, #field, table);

#define SERIALIZE(name, ...) \
    template<> \
    void LoadValue<name>(name* dest, toml::node* data) \
    { \
         toml::table* table = data->as_table(); \
         FOR_FIELDS(ADD_FIELD, name, __VA_ARGS__) \
    } \

#define COMPONENT(type) [[maybe_unused]] static int add##type = (AddComponent<type>(#type), 0);

// Define the game's components here

SERIALIZE(Transform3D, position, rotation, scale)
COMPONENT(Transform3D)
template <>
void LoadComponent<Transform3D>(Scene &scene, EntityID entity, toml::table* compData)
{
    Transform3D* comp = scene.Get<Transform3D>(entity);
    LoadValue<Transform3D>(comp, compData);

    if (compData->contains("parent"))
    {
        toml::node* parentData = compData->get("parent");
        if (!parentData->is_string())
        {
            std::cout << "This field must be an string\n";
        }
        std::string parentName = parentData->as_string()->get();
        if (!entityNames.contains(parentName))
        {
            std::cout << "This field must be the name of an entity";
        }
        EntityID parent = entityNames[parentName];
        Transform3D* parentTransform = scene.Get<Transform3D>(parent);
        if (parentTransform == nullptr)
        {
            std::cout << "The parent must have a Transform3D";
        }
        comp->SetParent(parentTransform);
    }
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
    float fov = 90;
    float nearPlane = 0.01;
    float farPlane = 1000;
};
SERIALIZE(CameraComponent, fov, nearPlane, farPlane)
COMPONENT(CameraComponent)


struct FlyingMovement
{
    float moveSpeed = 5;
    float turnSpeed = 0.1;
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
    float width = 1;
    float length = 1;
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
COMPONENT(DirLight)


struct SpotLight
{
    glm::vec3 diffuse = glm::vec3{1};
    glm::vec3 specular = glm::vec3{1};
    LightID lightID = -1;

    float innerCone = 30;
    float outerCone = 45;
    float range = 100;
};
SERIALIZE(SpotLight, diffuse, specular, innerCone, outerCone, range)
COMPONENT(SpotLight)


struct PointLight
{
    glm::vec3 diffuse = glm::vec3{1};
    glm::vec3 specular = glm::vec3{1};
    LightID lightID = -1;

    float constant = 1;
    float linear = 0.25;
    float quadratic = 0.2;

    float maxRange = 20;
};
SERIALIZE(PointLight, diffuse, specular, constant, linear, quadratic, maxRange)
COMPONENT(PointLight)
