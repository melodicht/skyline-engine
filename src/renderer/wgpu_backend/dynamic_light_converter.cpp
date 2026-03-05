#include <dynamic_light_converter.h>

#include <glm/gtc/matrix_transform.hpp>
#include <skl_math_utils.h>

// DEBUG
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

// NOTE: THIS ASSUMES A SYMMETRIC PERSPECTIVE MATRIX IS USED FOR PROJECTION
// This prepares gpu side directional lights.
// Light spaces are added on per cascade, 
// (i.e. if lightSpacesCascadeCount == 2 and cpuType comprised of {a,b} then the added lightSpaces would be {(a cascade 1), (b cascade 1), (a cascade 2), (b cascade 2)})
std::vector<WGPUBackendDynamicShadowedDirLightData> DynamicLightConverter::ConvertDirLights(
    std::vector<DirLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    const glm::mat4x4& camPerspectiveMat,
    const glm::mat4x4& camViewMat,
    const std::vector<f32>& cascadeRatios,
    f32 cascadeBleed,
    u32 mapSquareResolution,
    f32 camNear,
    f32 camFar) {
    glm::vec3 camScale = GetScaleFromView(camViewMat);
    glm::vec3 camTranslate = GetWorldTranslateFromView(camViewMat);

    glm::vec3 camScaleDelta = camScale - m_prevScale;
    // Checks if camera frustum dimension have changed
    // Adjusts sizes in m_cascadeRadii if so
    if (cascadeRatios != m_prevCascadeRatios 
        || camPerspectiveMat != m_prevPerspective
        || glm::dot(camScaleDelta,camScaleDelta) >= 0.001
        || cascadeBleed != m_prevBleed) {
        m_prevCascadeRatios = cascadeRatios;
        m_prevPerspective = camPerspectiveMat;
        m_prevScale = camScale;
        m_prevBleed = cascadeBleed;

        glm::mat4x4 invertedPerspectiveSpace = glm::inverse(camPerspectiveMat);
        glm::vec4 camScaleFour = glm::vec4(camScale,1.0f);

        // Find corner dimensions of perspective projection (with view mat scale)
        glm::vec4 nearCornerPoint = invertedPerspectiveSpace * glm::vec4(1,1,0,1);
        nearCornerPoint /= nearCornerPoint.w;
        nearCornerPoint *= camScaleFour;
        glm::vec4 nearToFarCorner = invertedPerspectiveSpace * glm::vec4(1,1,1,1);
        nearToFarCorner /= nearToFarCorner.w;
        nearToFarCorner *= camScaleFour;
        nearToFarCorner -= nearCornerPoint;

        // Find near and far mid point dimensions of perspective project (with view mat scale)
        glm::vec4 nearMidPoint = invertedPerspectiveSpace * glm::vec4(0,0,0,1);
        nearMidPoint /= nearMidPoint.w;
        nearMidPoint *= camScaleFour;
        glm::vec4 nearToFarMid = invertedPerspectiveSpace * glm::vec4(0,0,1,1);
        nearToFarMid /= nearToFarMid.w;
        nearToFarMid *= camScaleFour;
        nearToFarMid -= nearMidPoint;

        m_nearPlaneDistance = nearMidPoint.z;
        m_farPlaneDistance = m_nearPlaneDistance + nearToFarMid.z;

        // Find scales from each subdivision
        m_cascadeRadii.clear();
        m_cascadeRadii.reserve(cascadeRatios.size());
        f32 lastRange = 0;
        for (s32 divisionIter = 0 ; divisionIter < cascadeRatios.size() ; divisionIter++) {
            f32 divisionEnd = cascadeRatios[divisionIter];
            f32 adjustedDivisionEnd = divisionEnd + cascadeBleed;
            f32 midRange = (lastRange + divisionEnd)/2;

            m_cascadeRadii.push_back(
                glm::distance( 
                    nearCornerPoint + (nearToFarCorner * adjustedDivisionEnd),
                    nearMidPoint + (nearToFarMid * midRange)));

            lastRange = divisionEnd;
        }
    }

    // Organizes and reserves information
    std::vector<WGPUBackendDynamicShadowedDirLightData> ret;
    std::vector<glm::mat3x3> invertedLightRots;
    ret.reserve(cpuType.size());
    invertedLightRots.reserve(cpuType.size());

    // Inserts non light space data into GPU data
    for (DirLightRenderInfo& cpuDat : cpuType) {
        WGPUBackendDynamicShadowedDirLightData gpuDat{ };
        gpuDat.m_diffuse = cpuDat.diffuse;
        gpuDat.m_intensity = 1; // TODO: Implement intensity scaling
        gpuDat.m_specular = cpuDat.specular;
        gpuDat.m_direction = cpuDat.transform->GetForwardVector();

        ret.push_back(std::move(gpuDat));
        // Since rot mat is orthonormal inverse can be found from transpose
        invertedLightRots.push_back(
            GetRotMat(cpuDat.transform->GetViewMatrix()));
    }

    // Cam space forward direction
    glm::vec3 camDir = GetForwardVecFromView(camViewMat);

    // Find near and far mid point dimensions of perspective project in world space
    glm::vec3 nearMidPoint = (camDir * camNear) + camTranslate;
    glm::vec3 nearToFarMid = camDir * (camFar - camNear);
    f32 lastCascade = 0;
    for (int cascadeIter = 0 ; cascadeIter < cascadeRatios.size() ; cascadeIter++) {
        f32 midCascade = (cascadeRatios[cascadeIter] + lastCascade) / 2;
        
        // Orthographic view that covers entirety of the matrix 
        f32 cascadeRadius = m_cascadeRadii[cascadeIter];
        glm::mat4x4 projectionMat = glm::ortho(
            -cascadeRadius, cascadeRadius, 
            -cascadeRadius, cascadeRadius,
            -cascadeRadius, cascadeRadius);

        // Gets center of cascade
        f32 texelsPerUnit = mapSquareResolution / (cascadeRadius * 2.0f);
        glm::vec3 cascadeCenter = nearMidPoint + nearToFarMid * midCascade;

        for (glm::mat3x3 invertedLightRot : invertedLightRots) {
            // Quantizes center then inserts into light space
            // Multiplies by invertedLightRot then negates in order to find view space from camera transform
            // Equation of R^-1 | -R^-1(t)
            glm::vec3 lightSpaceCascadeCenter = invertedLightRot * cascadeCenter;

            lightSpaceCascadeCenter.x = glm::floor(lightSpaceCascadeCenter.x * texelsPerUnit) / texelsPerUnit;
            lightSpaceCascadeCenter.y = glm::floor(lightSpaceCascadeCenter.y * texelsPerUnit) / texelsPerUnit;

            glm::mat4x4 lightSpace = glm::mat4x4(invertedLightRot);
            lightSpace[3] = glm::vec4(-lightSpaceCascadeCenter, 1.0f);

            lightSpacesOutput.push_back(projectionMat * lightSpace);
        }
        lastCascade = cascadeRatios[cascadeIter];
    }
    return ret;
}

local glm::mat4x4 lookAtHelper(glm::vec3 location, glm::vec3 forward, glm::vec3 up) {
    return glm::lookAt(location,location + forward, up);
}

// Converts cpu point lights to gpu side point lights.
std::vector<WGPUBackendDynamicShadowedPointLightData> DynamicLightConverter::ConvertPointLights(
    std::vector<PointLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    s32 shadowHeight,
    s32 shadowWidth) {

    std::vector<WGPUBackendDynamicShadowedPointLightData> ret{ };
    ret.reserve(cpuType.size());
    for (PointLightRenderInfo& cpuDat : cpuType) {
        glm::vec3 lightPos = cpuDat.transform->GetWorldPosition();

        // Calculates cube map 
        glm::mat4x4 proj = glm::perspective(glm::radians(90.0f), (f32)shadowWidth/(f32)shadowHeight, 0.1f, cpuDat.radius);
        // X faces
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 1, 0, 0}, { 0, 1, 0}));
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, {-1, 0, 0}, { 0, 1, 0}));
        // Y faces
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0, 1, 0}, { 0, 0,-1}));
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0,-1, 0}, { 0, 0, 1}));
        // Z faces
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0, 0, 1}, { 0, 1, 0}));
        lightSpacesOutput.push_back(proj * lookAtHelper(lightPos, { 0, 0,-1}, { 0, 1, 0}));
        // Populates gpu type information 
        WGPUBackendDynamicShadowedPointLightData gpuDat{ };

        gpuDat.m_diffuse = cpuDat.diffuse;
        gpuDat.m_specular = cpuDat.specular;
        gpuDat.m_position = lightPos;
        gpuDat.m_falloff = cpuDat.falloff;
        gpuDat.m_radius = cpuDat.radius;

        ret.push_back(gpuDat);
    }
    return ret;
}

std::vector<WGPUBackendDynamicShadowedSpotLightData> DynamicLightConverter::ConvertSpotLights(std::vector<SpotLightRenderInfo>& cpuType) {
    std::vector<WGPUBackendDynamicShadowedSpotLightData> ret{ };
    ret.reserve(cpuType.size());

    for (SpotLightRenderInfo& cpuDat : cpuType) {
        WGPUBackendDynamicShadowedSpotLightData gpuDat{ };

        gpuDat.m_diffuse = cpuDat.diffuse;
        gpuDat.m_penumbraCutoff = cpuDat.innerCone;
        gpuDat.m_specular = cpuDat.specular;
        gpuDat.m_outerCutoff = cpuDat.outerCone;
        gpuDat.m_direction = cpuDat.transform->GetForwardVector();
        gpuDat.m_position = cpuDat.transform->GetWorldPosition();

        ret.push_back(gpuDat);
    }
    return ret;
} 
