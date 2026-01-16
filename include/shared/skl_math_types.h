#pragma once

#include <unordered_set>

#include <glm/glm.hpp>

#include <meta_definitions.h>

// This represents constants/typedefs often used throughout program
// Really any compile time math concept

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
    glm::vec3 GetWorldPosition();
    glm::vec3 GetForwardVector();
    glm::vec3 GetRightVector();
    glm::vec3 GetUpVector();
    glm::mat4 GetViewMatrix();
    void GetPointViews(glm::mat4 *views);
    void SetParent(Transform3D *newParent);
    Transform3D *GetParent();
    void MarkDirty();

    ~Transform3D();

private:
    bool dirty = true;
    Transform3D *parent;
    std::unordered_set<Transform3D *> children;
    glm::mat4 worldTransform;
};
