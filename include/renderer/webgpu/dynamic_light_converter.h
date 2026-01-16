#pragma once

#include <render_types_wgpu.h>
#include <render_backend.h>

#include <meta_definitions.h>

#include <cassert>
#include <vector>
#include <utility>
#include <functional>

// This prepares gpu side directional lights.
// Light spaces are added on per cascade, 
// (i.e. if lightSpacesCascadeCount == 2 and cpuType comprised of {a,b} then the added lightSpaces would be {(a cascade 1), (b cascade 1), (a cascade 2), (b cascade 2)})
std::vector<WGPUBackendDynamicShadowedDirLightData> ConvertDirLights(
    std::vector<DirLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    s32 lightSpacesCascadeCount,
    const glm::mat4x4& camSpaceMat,
    const std::vector<float>& cascadeRatios,
    float cascadeBleed,
    float camFar);

// Converts cpu point lights to gpu side point lights.
std::vector<WGPUBackendDynamicShadowedPointLightData> ConvertPointLights(
    std::vector<PointLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    s32 shadowHeight,
    s32 shadowWidth);

std::vector<WGPUBackendDynamicShadowedSpotLightData> ConvertSpotLights(std::vector<SpotLightRenderInfo>& cpuType);