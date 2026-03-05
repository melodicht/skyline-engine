#pragma once

#include <render_types_wgpu.h>
#include <render_backend.h>

#include <meta_definitions.h>

#include <cmath>
#include <vector>
#include <utility>
#include <functional>
#include <utility>

class DynamicLightConverter {
private:
    // >>> Dir Light Fields <<<
    // Information used to inform resizing directional lights
    // Stored to prevent constant resizing of maps
    glm::mat4x4 m_prevPerspective{ NAN };
    std::vector<float> m_prevCascadeRatios{ NAN };
    std::vector<float> m_cascadeRadii{};
    glm::vec3 m_prevScale{NAN, NAN, NAN}; 
    f32 m_prevBleed{ NAN };
    f32 m_nearPlaneDistance{ NAN };
    f32 m_farPlaneDistance{ NAN };
public:
    // This prepares gpu side directional lights.
    // Light spaces are added on per cascade, 
    // (i.e. if lightSpacesCascadeCount == 2 and cpuType comprised of {a,b} then the added lightSpaces would be {(a cascade 1), (b cascade 1), (a cascade 2), (b cascade 2)})
    std::vector<WGPUBackendDynamicShadowedDirLightData> ConvertDirLights(
        std::vector<DirLightRenderInfo>& cpuType,
        std::vector<glm::mat4x4>& lightSpacesOutput,
        const glm::mat4x4& camPerspectiveMat,
        const glm::mat4x4& camViewMat,
        const std::vector<float>& cascadeRatios,
        f32 cascadeBleed,
        u32 mapSquareResolution,
        f32 camNear,
        f32 camFar);

    // Converts cpu point lights to gpu side point lights.
    std::vector<WGPUBackendDynamicShadowedPointLightData> ConvertPointLights(
        std::vector<PointLightRenderInfo>& cpuType,
        std::vector<glm::mat4x4>& lightSpacesOutput,
        s32 shadowHeight,
        s32 shadowWidth);

    std::vector<WGPUBackendDynamicShadowedSpotLightData> ConvertSpotLights(std::vector<SpotLightRenderInfo>& cpuType);
};