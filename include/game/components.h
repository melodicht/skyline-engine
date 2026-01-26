#pragma once
#include <glm/glm.hpp>

#include <meta_definitions.h>
#include <skl_math_types.h>
#include <asset_types.h>
#include <component_registry.h>

// Define the game's components here

SERIALIZE(Transform3D, position, rotation, scale)
COMPONENT(Transform3D)

struct MeshComponent
{
    MeshAsset* mesh;
    TextureAsset* texture;
    glm::vec3 color = glm::vec3{1.0f};
    bool dirty = true;
};
SERIALIZE(MeshComponent, mesh, texture, color)
COMPONENT(MeshComponent)

namespace JPH
{
    class CharacterVirtual;
}

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


struct BuilderPlane
{
    f32 width = 1;
    f32 length = 1;
};
SERIALIZE(BuilderPlane, width, length)
COMPONENT(BuilderPlane)


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

    f32 radius{ 0 };
    f32 falloff{ 0 };
};
SERIALIZE(PointLight, diffuse, specular, radius, falloff)
COMPONENT(PointLight, "gizmos/point_light")

struct Spin
{
    f32 speed = 20.0f;
};
SERIALIZE(Spin, speed)
COMPONENT(Spin)

#ifdef MARVIN_GAME

enum GravityBallStage
{
    gravityBallStage_growing,
    gravityBallStage_prime,
    gravityBallStage_decaying,
};

namespace JPH
{
    class Body;
}

struct GravityBall
{
    JPH::Body* body = nullptr;
    f32 life;
    f32 primeCounter;
    GravityBallStage stage = gravityBallStage_growing;
};
SERIALIZE(GravityBall)
COMPONENT(GravityBall)
#endif

COMPONENT(NameComponent)
