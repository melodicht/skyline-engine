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


// Generates a random float in the inclusive range of the two given
// floats.
f32 RandInBetween(f32 LO, f32 HI);

u32 RandInt(u32 min, u32 max);

// Generates a normalized vector orthogonal to the given one arbitrary (no guarantee of anything else other than orthogonal normalized nature)
glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec);

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right);

std::vector<glm::vec4> GetFrustumCorners(const glm::mat4& proj, const glm::mat4& view);