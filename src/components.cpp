#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "renderer/render_types.h"
#include "math/skl_math_consts.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Character/CharacterVirtual.h>

#define PARENS ()

#define EXPAND(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define ADD_FIELDS(type, ...) \
    __VA_OPT__(EXPAND(_ADD_FIELDS(type, __VA_ARGS__)))
#define _ADD_FIELDS(type, a1, ...) \
    LoadIfPresent(dest + offsetof(type, a1), #a1, table, LoadValue<decltype(type::a1)>); \
    __VA_OPT__(__ADD_FIELDS PARENS (type, __VA_ARGS__))
#define __ADD_FIELDS() _ADD_FIELDS

#define SERIALIZE(name, ...) \
    template<> \
    void LoadValue<name>(char* dest, toml::node* data) \
    { \
         toml::table* table = data->as_table(); \
         ADD_FIELDS(name, __VA_ARGS__) \
    } \

// Define the game's components here

struct MeshComponent
{
    MeshID mesh = -1;
    TextureID texture = -1;
    glm::vec3 color = glm::vec3{1.0f};
    bool dirty = true;
};
SERIALIZE(MeshComponent, mesh, texture, color)

struct PlayerCharacter
{
    JPH::CharacterVirtual* characterVirtual = nullptr;
};

struct CameraComponent
{
    float fov = 90;
    float nearPlane = 0.01;
    float farPlane = 1000;
};
SERIALIZE(CameraComponent, fov, nearPlane, farPlane)

struct FlyingMovement
{
    float moveSpeed = 5;
    float turnSpeed = 0.1;
};
SERIALIZE(FlyingMovement, moveSpeed, turnSpeed)

struct Plane
{
    float width = 1;
    float length = 1;
};
SERIALIZE(Plane, width, length)

struct DirLight
{
    glm::vec3 diffuse = glm::vec3{1};
    glm::vec3 specular = glm::vec3{1};
    LightID lightID = -1;
};
SERIALIZE(DirLight, diffuse, specular)

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