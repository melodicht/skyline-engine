#include <dynamic_light_converter.h>

#include <glm/gtc/matrix_transform.hpp>
#include <skl_math_utils.h>
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
    float camFar) {

    // Organizes and reserves information
    std::vector<WGPUBackendDynamicShadowedDirLightData> ret;
    std::vector<glm::mat4x4> lightViews;
    ret.reserve(cpuType.size());
    lightViews.reserve(cpuType.size());

    // Inserts non light space data into GPU data
    for (DirLightRenderInfo& cpuDat : cpuType) {
        WGPUBackendDynamicShadowedDirLightData gpuDat{ };
        gpuDat.m_diffuse = cpuDat.diffuse;
        gpuDat.m_intensity = 1; // TODO: Implement intensity scaling
        gpuDat.m_specular = cpuDat.specular;
        gpuDat.m_direction = cpuDat.transform->GetForwardVector();

        ret.push_back(std::move(gpuDat));
        lightViews.push_back(std::move(cpuDat.transform->GetViewMatrix()));
    }

    // Find world corners of camera space
    glm::mat4x4 invertedCamSpace = glm::inverse(camSpaceMat);
    std::array<glm::vec4, 4> nearCorners;
    std::array<glm::vec4, 4> nearToFarCornerVectors;
    u8 cornerIter = 0;
    for (s8 x = -1 ; x <= 1 ; x += 2) {
        for (s8 y = -1 ; y <= 1 ; y += 2) {
            nearCorners[cornerIter] = invertedCamSpace * glm::vec4(x,y,0,1);
            nearCorners[cornerIter] /= nearCorners[cornerIter].w;
            
            glm::vec4 farCorner = invertedCamSpace * glm::vec4(x,y,1,1);
            farCorner /= farCorner.w;

            nearToFarCornerVectors[cornerIter] = farCorner - nearCorners[cornerIter];
            cornerIter++;
        }
    }

    // The cascade inserted should contain the same amount of ratios as the amount of cascades
    assert(cascadeRatios.size() == lightSpacesCascadeCount);

    for (s32 cascadeIterator = 0; cascadeIterator < lightSpacesCascadeCount; cascadeIterator++)
    {
        float startRatio;
        if (cascadeIterator == 0) {
            startRatio = 0;
        }
        else {
            startRatio = cascadeRatios[cascadeIterator - 1];
        }

        float trueStartRatio = startRatio - cascadeBleed;
        float trueEndRatio = cascadeRatios[cascadeIterator] + cascadeBleed;
        
        std::array<glm::vec4, 8> corners;
        u8 cornerIter = 0;
        u8 wholeCornerIter = 0;
        for (s8 x = -1 ; x <= 1 ; x += 2) {
            for (s8 y = -1 ; y <= 1 ; y += 2) {
                corners[cornerIter] = nearCorners[wholeCornerIter] + nearToFarCornerVectors[wholeCornerIter] * trueStartRatio;
                corners[cornerIter].w = 1;
                cornerIter++;

                corners[cornerIter] = nearCorners[wholeCornerIter] + nearToFarCornerVectors[wholeCornerIter] * trueEndRatio;
                corners[cornerIter].w = 1;
                cornerIter++;
                wholeCornerIter++;
            }
        }

        for (s32 cpuIter = 0; cpuIter < cpuType.size() ; cpuIter++) {
            f32 minX = std::numeric_limits<f32>::max();
            f32 maxX = std::numeric_limits<f32>::lowest();
            f32 minY = std::numeric_limits<f32>::max();
            f32 maxY = std::numeric_limits<f32>::lowest();
            f32 minZ = std::numeric_limits<f32>::max();
            f32 maxZ = std::numeric_limits<f32>::lowest();

            for (const glm::vec4& v : corners) {
                glm::vec4 trf = lightViews[cpuIter] * v;
                trf /= trf.w;
                minX = std::min(minX, trf.x);
                maxX = std::max(maxX, trf.x);
                minY = std::min(minY, trf.y);
                maxY = std::max(maxY, trf.y);
                maxZ = std::max(maxZ, trf.z);
                minZ = std::min(minZ, trf.z);
            }

            // TODO: Find more specific method for determining distance from frustum (MinZ)
            glm::mat4 dirProj = glm::ortho(minX, maxX, minY, maxY, minZ - camFar, maxZ);  
            
            // Inserts dir light into gpu vector   
            lightSpacesOutput.push_back(dirProj * lightViews[cpuIter]);
        }
    }
    return ret;
}

inline glm::mat4x4 lookAtHelper(glm::vec3 location, glm::vec3 forward, glm::vec3 up) {
    return glm::lookAt(location,location + forward, up);
}

// Converts cpu point lights to gpu side point lights.
std::vector<WGPUBackendDynamicShadowedPointLightData> ConvertPointLights(
    std::vector<PointLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    s32 shadowHeight,
    s32 shadowWidth) {

    std::vector<WGPUBackendDynamicShadowedPointLightData> ret{ };
    ret.reserve(cpuType.size());
    for (PointLightRenderInfo& cpuDat : cpuType) {
        glm::vec3 lightPos = cpuDat.transform->position;

        // Calculates cube map 
        glm::mat4x4 proj = glm::perspective(glm::radians(90.0f), (float)shadowWidth/(float)shadowHeight, 0.1f, cpuDat.maxRange);
        // X faces
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 1, 0, 0}, { 0, 1, 0}));
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, {-1, 0, 0}, { 0, 1, 0}));
        // Y faces
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0, 1, 0}, { 0, 0, 1}));
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0,-1, 0}, { 0, 0,-1}));
        // Z faces
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0, 0, 1}, { 0, 1, 0}));
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0, 0,-1}, { 0, 1, 0}));
        // Populates gpu type information 
        WGPUBackendDynamicShadowedPointLightData gpuDat{ };

        gpuDat.m_diffuse = cpuDat.diffuse;
        gpuDat.m_constant = cpuDat.constant; 
        gpuDat.m_specular = cpuDat.specular;
        gpuDat.m_linear = cpuDat.linear;
        gpuDat.m_position = lightPos;
        gpuDat.m_quadratic = cpuDat.quadratic;
        gpuDat.m_distanceCutoff = cpuDat.maxRange;

        ret.push_back(gpuDat);
    }
    return ret;
}

std::vector<WGPUBackendDynamicShadowedSpotLightData> ConvertSpotLights(std::vector<SpotLightRenderInfo>& cpuType) {
    std::vector<WGPUBackendDynamicShadowedSpotLightData> ret{ };
    ret.reserve(cpuType.size());

    for (SpotLightRenderInfo& cpuDat : cpuType) {
        WGPUBackendDynamicShadowedSpotLightData gpuDat{ };

        gpuDat.m_diffuse = cpuDat.diffuse;
        gpuDat.m_penumbraCutoff = cpuDat.innerCone;
        gpuDat.m_specular = cpuDat.specular;
        gpuDat.m_outerCutoff = cpuDat.outerCone;
        gpuDat.m_direction = cpuDat.transform->GetForwardVector();
        gpuDat.m_position = cpuDat.transform->position;

        ret.push_back(gpuDat);
    }
    return ret;
} 
