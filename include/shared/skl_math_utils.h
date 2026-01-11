#pragma once

#include <glm/glm.hpp>

#include <skl_math_types.h>

glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);


// Generates a random float in the inclusive range of the two given
// floats.
f32 RandInBetween(f32 LO, f32 HI);

u32 RandInt(u32 min, u32 max);

// Generates a normalized vector orthogonal to the given one arbitrary (no guarantee of anything else other than orthogonal normalized nature)
glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);

std::vector<glm::vec4> GetFrustumCorners(const glm::mat4& proj, const glm::mat4& view);