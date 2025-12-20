#include "skl_math_utils.h"

#include <random>
#include <iostream>

glm::vec3 Transform3D::GetLocalPosition()
{
    return this->position;
}
void Transform3D::SetLocalPosition(glm::vec3 newPos)
{
    this->position = newPos;
    MarkDirty();
}
void Transform3D::AddLocalPosition(glm::vec3 offset)
{
    this->position += offset;
    MarkDirty();
}
glm::vec3 Transform3D::GetLocalRotation()
{
    return rotation;
}
void Transform3D::SetLocalRotation(glm::vec3 newRot)
{
    this->rotation = newRot;
    MarkDirty();
}
void Transform3D::AddLocalRotation(glm::vec3 offset)
{
    this->rotation += offset;
    MarkDirty();
}
glm::vec3 Transform3D::GetLocalScale()
{
    return this->scale;
}
void Transform3D::SetLocalScale(glm::vec3 newScale)
{
    this->scale = newScale;
    MarkDirty();
}

void Transform3D::MarkDirty()
{
    if (!this->dirty)
    {
        this->dirty = true;
        for (Transform3D *child : children)
        {
            child->MarkDirty();
        }
    }
}

glm::mat4 Transform3D::GetWorldTransform()
{
    if (this->dirty)
    {
        glm::quat aroundX = glm::angleAxis(glm::radians(this->rotation.x), glm::vec3(1.0, 0.0, 0.0));
        glm::quat aroundY = glm::angleAxis(glm::radians(this->rotation.y), glm::vec3(0.0, 1.0, 0.0));
        glm::quat aroundZ = glm::angleAxis(glm::radians(this->rotation.z), glm::vec3(0.0, 0.0, 1.0));
        glm::mat4 rotationMat = glm::mat4_cast(aroundZ * aroundY * aroundX);

        this->dirty = false;

        this->worldTransform = glm::scale(glm::translate(glm::mat4(1.0f), this->position) * rotationMat, this->scale);

        if (parent != nullptr)
        {
            this->worldTransform = this->parent->GetWorldTransform() * this->worldTransform;
        }
    }

    return this->worldTransform;
}

glm::vec3 Transform3D::GetForwardVector()
{
    return GetWorldTransform() * glm::vec4(1.0, 0.0, 0.0, 0.0);
}

glm::vec3 Transform3D::GetRightVector()
{
    return GetWorldTransform() * glm::vec4(0.0, 1.0, 0.0, 0.0);
}

glm::vec3 Transform3D::GetUpVector()
{
    return GetWorldTransform() * glm::vec4(0.0, 0.0, 1.0, 0.0);
}

glm::mat4 MakeViewMatrix(glm::vec3 forward, glm::vec3 right, glm::vec3 up, glm::vec3 position)
{
    glm::mat4 view = {};
    view[0] = {right.x, up.x, forward.x, 0};
    view[1] = {right.y, up.y, forward.y, 0};
    view[2] = {right.z, up.z, forward.z, 0};
    view[3] = {-glm::dot(right, position),
               -glm::dot(up, position),
               -glm::dot(forward, position),
               1};

    return view;
}

glm::mat4 Transform3D::GetViewMatrix()
{
    glm::vec3 forward = GetForwardVector();
    glm::vec3 right = GetRightVector();
    glm::vec3 up = GetUpVector();

    return MakeViewMatrix(forward, right, up, GetWorldTransform() * glm::vec4(0, 0, 0, 1));
}

void Transform3D::GetPointViews(glm::mat4 *views)
{
    glm::vec3 forward = {1, 0, 0};
    glm::vec3 right = {0, 1, 0};
    glm::vec3 up = {0, 0, 1};

    glm::vec3 worldPosition = GetWorldTransform() * glm::vec4(0, 0, 0, 1);

    views[0] = MakeViewMatrix(forward, -up, right, worldPosition);
    views[1] = MakeViewMatrix(-forward, up, right, worldPosition);
    views[2] = MakeViewMatrix(right, forward, -up, worldPosition);
    views[3] = MakeViewMatrix(-right, forward, up, worldPosition);
    views[4] = MakeViewMatrix(up, forward, right, worldPosition);
    views[5] = MakeViewMatrix(-up, -forward, right, worldPosition);
}

void Transform3D::SetParent(Transform3D *newParent)
{
    if (this->parent != nullptr)
    {
        this->parent->children.erase(this);
    }

    this->parent = newParent;
    newParent->children.insert(this);
    MarkDirty();
}

Transform3D *Transform3D::GetParent()
{
    return this->parent;
}

Transform3D::~Transform3D()
{
    if (this->parent != nullptr)
    {
        this->parent->children.erase(this);
    }

    for (Transform3D *child : children)
    {
        child->parent = nullptr;
    }
}

// Generates a random float in the inclusive range of the two given
// floats.
f32 RandInBetween(f32 LO, f32 HI)
{
    // From https://stackoverflow.com/questions/686353/random-float-number-generation
    return LO + static_cast<f32>(rand()) / (static_cast<f32>(RAND_MAX / (HI - LO)));
}

u32 RandInt(u32 min, u32 max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<u32> distribution(min, max);
    return distribution(gen);
}

glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec) {
  if (std::abs(vec.x) < std::abs(vec.y) && std::abs(vec.x) < std::abs(vec.z)) {
    return glm::normalize(glm::cross(vec, glm::vec3(1, 0, 0)));
  } else if (abs(vec.y) < abs(vec.z)) {
    return glm::normalize(glm::cross(vec, glm::vec3(0, 1, 0)));
  } else {
    return glm::normalize(glm::cross(vec, glm::vec3(0, 0, 1)));
  }
}

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& left) {
  return glm::mat4x4(
    glm::vec4(left, 0.0f),
    glm::vec4(up, 0.0f),
    glm::vec4(forward, 0.0f),
    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
  );
}

std::vector<glm::vec4> GetFrustumCorners(const glm::mat4& proj, const glm::mat4& view)
{
    glm::mat4 inverse = glm::inverse(proj * view);

    std::vector<glm::vec4> frustumCorners;
    for (u32 x = 0; x < 2; ++x)
    {
        for (u32 y = 0; y < 2; ++y)
        {
            for (u32 z = 0; z < 2; ++z)
            {
                const glm::vec4 pt =
                        inverse * glm::vec4(
                                2.0f * x - 1.0f,
                                2.0f * y - 1.0f,
                                z,
                                1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}