#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <skl_math_types.h>

glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);

glm::mat4x4 GetRotMat(const glm::mat4x4& mat);

glm::vec3 GetScaleFromView(const glm::mat4x4& viewMat);

glm::vec3 GetWorldTranslateFromView(const glm::mat4x4& viewMat);

glm::vec3 GetForwardVecFromView(const glm::mat4x4& viewMat);

void ScaleMatrix(glm::mat4x4& mat, const glm::vec3& scaleVec);

// Generates a random float in the inclusive range of the two given
// floats.
f32 RandInBetween(f32 LO, f32 HI);

u32 RandInt(u32 min, u32 max);

// Generates a normalized vector orthogonal to the given one arbitrary (no guarantee of anything else other than orthogonal normalized nature)
glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);

std::vector<glm::vec4> GetFrustumCorners(const glm::mat4& proj, const glm::mat4& view);