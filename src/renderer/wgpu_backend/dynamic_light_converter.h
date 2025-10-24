#pragma once

#include "skl_logger.h"
#include "math/skl_math_utils.h"

#include <cassert>
#include <vector>
#include <utility>
#include <functional>

#include "renderer/wgpu_backend/render_types_wgpu.h"
#include "renderer/render_backend.h"

// This prepares gpu side directional lights.
// Light spaces are added on per cascade, 
// (i.e. if lightSpacesCascadeCount == 2 and cpuType comprised of {a,b} then the added lightSpaces would be {(a cascade 1), (b cascade 1), (a cascade 2), (b cascade 2)})
std::vector<WGPUBackendDynamicShadowedDirLightData> ConvertDirLights(
    std::vector<DirLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    int lightSpacesCascadeCount,
    const glm::mat4x4& camSpaceMat,
    const std::vector<float>& cascadeRatios,
    float cascadeBleed,
    float camFar);

// Converts cpu point lights to gpu side point lights.
std::vector<WGPUBackendDynamicShadowedPointLightData> ConvertPointLights(std::vector<PointLightRenderInfo>& cpuType);

std::vector<WGPUBackendDynamicShadowedSpotLightData> ConvertSpotLights(std::vector<SpotLightRenderInfo>& cpuType);