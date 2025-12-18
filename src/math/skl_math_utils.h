#pragma once

#include "skl_math_types.h"

#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);

glm::mat4 GetRotationMatrix(Transform3D *transform);

glm::mat4 GetTransformMatrix(Transform3D *transform);

glm::vec3 GetForwardVector(const glm::mat4x4& rotMat);

glm::vec3 GetForwardVector(Transform3D *transform);

glm::vec3 GetRightVector(const glm::mat4x4& rotMat);

glm::vec3 GetRightVector(Transform3D *transform);

glm::vec3 GetUpVector(const glm::mat4x4& rotMat);

glm::vec3 GetUpVector(Transform3D *transform);

glm::mat4 GetViewMatrix(Transform3D *transform);

void GetPointViews(Transform3D *transform, glm::mat4 *views);

// Generates a random float in the inclusive range of the two given
// floats.
f32 RandInBetween(f32 LO, f32 HI);

u32 RandInt(u32 min, u32 max);

// Generates a normalized vector orthogonal to the given one arbitrary (no guarantee of anything else other than orthogonal normalized nature)
glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);

std::vector<glm::vec4> GetFrustumCorners(const glm::mat4& proj, const glm::mat4& view);