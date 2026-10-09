#include <dynamic_light_converter.h>

#include <glm/gtc/matrix_transform.hpp>
#include <skl_math_utils.h>
#include <skl_debug.h>

// Only really needs to set near plane logic
DynamicLightConverter::DynamicLightConverter(f32 defaultNearPlaneDistance) : m_defaultNearPlaneDistance(defaultNearPlaneDistance) {}

// This prepares gpu side directional lights.
// Light spaces are added on per cascade, 
// (i.e. if lightSpacesCascadeCount == 2 and cpuType comprised of {a,b} then the added lightSpaces would be {(a cascade 1), (b cascade 1), (a cascade 2), (b cascade 2)})
std::vector<WGPUBackendDynamicShadowedDirLightData> DynamicLightConverter::ConvertDirLights(
    const std::vector<DirLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    std::vector<f32>& worldToTexCoordRatio,
    const glm::mat4x4& camPerspectiveMat,
    const glm::mat4x4& camViewMat,
    const std::vector<f32>& cascadeRatios,
    f32 cascadeBleed,
    u32 mapSquareResolution,
    f32 camNear,
    f32 camFar) {
    // Got confused on notes, symmetry in this context mean frustrum left right symmetry 
    // not matrix
        //ASSERT_PRINT(IsSymmetric(camPerspectiveMat), "Dir lights are assumed to have dynamic perspective matrices");

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

            f32 newRadii = glm::distance( 
                    nearCornerPoint + (nearToFarCorner * adjustedDivisionEnd),
                    nearMidPoint + (nearToFarMid * midRange));
            m_cascadeRadii.push_back( newRadii );

            lastRange = divisionEnd;

            worldToTexCoordRatio.push_back(1.0f / (2.0f * newRadii));
        }
    }

    // Organizes and reserves information
    std::vector<WGPUBackendDynamicShadowedDirLightData> ret;
    std::vector<glm::mat3x3> invertedLightRots;
    ret.reserve(cpuType.size());
    invertedLightRots.reserve(cpuType.size());

    // Inserts non light space data into GPU data
    for (const DirLightRenderInfo& cpuDat : cpuType) {
        WGPUBackendDynamicShadowedDirLightData gpuDat{ };
        gpuDat.m_color = cpuDat.diffuse; // TODO currently the color is a bit of an approximation
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
    const std::vector<PointLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput,
    s32 shadowHeight,
    s32 shadowWidth) {

    std::vector<WGPUBackendDynamicShadowedPointLightData> ret{ };
    ret.reserve(cpuType.size());
    for (const PointLightRenderInfo& cpuDat : cpuType) {
        glm::vec3 lightPos = cpuDat.transform->GetWorldPosition();

        // Calculates cube map 
        glm::mat4x4 proj = glm::perspective(
            glm::radians(90.0f), 
            (f32)shadowWidth/(f32)shadowHeight, 
            m_defaultNearPlaneDistance, 
            cpuDat.radius);
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

        gpuDat.m_color = cpuDat.diffuse;
        gpuDat.m_position = lightPos;
        gpuDat.m_falloff = cpuDat.falloff;
        gpuDat.m_radius = cpuDat.radius;

        ret.push_back(gpuDat);
    }
    return ret;
}

std::vector<WGPUBackendDynamicShadowedSpotLightData> DynamicLightConverter::ConvertSpotLights(
    const std::vector<SpotLightRenderInfo>& cpuType,
    std::vector<glm::mat4x4>& lightSpacesOutput) {
    std::vector<WGPUBackendDynamicShadowedSpotLightData> ret{ };
    ret.reserve(cpuType.size());

    for (const SpotLightRenderInfo& cpuDat : cpuType) {
        const float innerRadianCutoff = glm::radians(cpuDat.innerCone);
        const float outerRadianCutoff = glm::radians(cpuDat.outerCone);
        
        const float worldToTextureCoordSlope = 2 * glm::tan(outerRadianCutoff);
        // Assigns GPU side information
        WGPUBackendDynamicShadowedSpotLightData gpuDat{ };
        gpuDat.m_color = cpuDat.diffuse;
        gpuDat.m_penumbraCosCutoff = std::cos(innerRadianCutoff);
        gpuDat.m_direction = cpuDat.transform->GetForwardVector();
        gpuDat.m_outerCosCutoff = std::cos(outerRadianCutoff);
        gpuDat.m_position = cpuDat.transform->GetWorldPosition();
        gpuDat.m_range = cpuDat.range;
        gpuDat.m_falloff = cpuDat.falloff;
        gpuDat.m_nearPlaneDim = worldToTextureCoordSlope * m_defaultNearPlaneDistance;
        gpuDat.m_planeDimSlope = worldToTextureCoordSlope;
        ret.push_back(gpuDat);

        // Assigns lightspace 
        glm::mat4x4 view = lookAtHelper(
            gpuDat.m_position, 
            gpuDat.m_direction, 
            cpuDat.transform->GetUpVector());
        glm::mat4x4 proj = glm::perspective(
            outerRadianCutoff * 2, 
            1.0f, 
            m_defaultNearPlaneDistance, 
            gpuDat.m_range);
        lightSpacesOutput.push_back(proj * view);
    }
    
    return ret;
} 
