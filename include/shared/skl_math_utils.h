#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <skl_math_types.h>

inline f32 SKL_PI = 3.14159265359;

template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
b8 IsSymmetric(const glm::mat<C, R, T, Q>& mat) {
    if constexpr (C != R) {
        return false; 
    }
    else {
        constexpr uint64_t dim = C;
        for (int x = 0 ; x < dim ; x++) {
            for (int y = 0 ; y < x ; y++) {
                if (mat[x][y] != mat[y][x]) {
                    return false;
                }
            }
        } 
        return true;
    }
}

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

// This does have some issues at lower discs
std::vector<glm::vec2> BuildPoissonDisk(f32 diskRadius, u32 diskCount);

// Has issues with being to edge centered as time goes on.
std::vector<glm::vec2> BuildVogelDisk(f32 diskRadius, u32 diskCount);