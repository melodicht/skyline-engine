#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <unordered_set>

// This represents constants/typedefs often used throughout program
// Really any compile time math concept

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

typedef float f32;
typedef double f64;

class Transform3D
{
public:
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale = glm::vec3(1);

    glm::vec3 GetLocalPosition();
    void SetLocalPosition(glm::vec3 newPos);
    void AddLocalPosition(glm::vec3 offset);
    glm::vec3 GetLocalRotation();
    void SetLocalRotation(glm::vec3 newRot);
    void AddLocalRotation(glm::vec3 offset);
    glm::vec3 GetLocalScale();
    void SetLocalScale(glm::vec3 newScale);
    glm::mat4 GetWorldTransform();
    glm::vec3 GetForwardVector();
    glm::vec3 GetRightVector();
    glm::vec3 GetUpVector();
    glm::mat4 GetViewMatrix();
    void GetPointViews(glm::mat4 *views);
    void SetParent(Transform3D *newParent);

    ~Transform3D();

private:
    Transform3D *parent;
    std::unordered_set<Transform3D *> children;
    glm::mat4 worldTransform;
    bool dirty = true;

    void MarkDirty();
};
