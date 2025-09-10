#pragma once

#include "skl_logger.h"
#include "math/skl_math_utils.h"

#include <cassert>
#include <vector>
#include <utility>
#include <functional>

// Stores shadowed light source information.
// Allows for shadows to be called and updated with lightIds while keeping information in a 
// array memory layout so that light data can be directly copied into GPU.
template <typename GPUType, typename CPUType, typename... ConversionArgs >
class WGPUBackendDynamicLightVectorMap {
private:
    // The lightID at a idx should correspond to a GPUType at the same index.
    // Also lightIds should always be sorted from least to greatest
    std::vector<LightID> lightIds{ };
    std::vector<GPUType> shadowData{ };

protected:
    // Extracts lightID from vector map types
    virtual LightID GetCPULightID(const CPUType& cpuType) = 0;
    
    // Each index at cpuType should correspond to a index of a gpu type in cpuToGPUIndices that points to a GPUType in output with the same lightID.
    virtual void Convert(std::vector<CPUType>& cpuType, std::vector<int>& cpuToGPUIndices,std::vector<GPUType>& output, ConversionArgs... args) = 0;

public:
    WGPUBackendDynamicLightVectorMap() { }
    virtual ~WGPUBackendDynamicLightVectorMap() { }

    // This makes the assumption that the newest lightId added is the largest so that there does not need to be any process of sorting
    void PushBack(LightID id) {
        lightIds.push_back(id);
        // Creates a dummy GPUType at the back to be overwritten later
        shadowData.emplace_back();
    }

    void Erase(LightID lightId) {
        // Finds lightIter with assumption of being sorted
        std::vector<LightID>::iterator lightIter = std::lower_bound(lightIds.begin(), lightIds.end(), lightId, std::less<u32>());

        if (lightIter != lightIds.end() && *lightIter == lightId) {
            shadowData.erase(shadowData.begin() + std::distance(lightIds.begin(), lightIter));
            lightIds.erase(lightIter);    
        }
        else {
            LOG_ERROR("Light ID that was erased never existed or lied in unsorted vector");
        }
    }

    // This functions works under the assumption that no unregistered shadows  will come in through shadowUpdate.
    // However if a registered shadow id is not present in shadowUpdate, the shadow will simply not update.
    void Update(std::vector<CPUType>& shadowUpdate, ConversionArgs... args) {
        std::sort(shadowUpdate.begin(), shadowUpdate.end(), [this](const CPUType& a, const CPUType& b) {
            return this->GetCPULightID(a) < this->GetCPULightID(b);
        });

        std::vector<int> indices;

        auto gpuIdsIter = lightIds.begin();
        auto shadowIter = shadowUpdate.begin();
        while (shadowIter != shadowUpdate.end() && gpuIdsIter != lightIds.end()) {
            if (*gpuIdsIter == GetCPULightID(*shadowIter)) {
                indices.push_back((int)std::distance(lightIds.begin(), gpuIdsIter));
                shadowIter++;
            }
            gpuIdsIter++;
        }

        assert(indices.size() == shadowUpdate.size());
        Convert(shadowUpdate, indices, shadowData, args...);
    }

    const std::vector<GPUType>& GetShadowData() {
        return shadowData;
    }
};

#include "renderer/wgpu_backend/render_types_wgpu.h"
#include "renderer/render_backend.h"

// TODO: Change to informed amount
#define DefaultCascade 4

template <size_t CascadeSize>
class WGPUBackendDynamicDirectionalLightMap : public WGPUBackendDynamicLightVectorMap<
        WGPUBackendDynamicShadowedDirLightData<CascadeSize>,
        DirLightRenderInfo, 
        const glm::mat4x4&,
        const std::vector<float>&,
        float,
        float> {
protected:
    LightID GetCPULightID(const DirLightRenderInfo& cpuType) override final {
        return cpuType.lightID;
    }

    void Convert(
        std::vector<DirLightRenderInfo>& cpuType,
        std::vector<int>& cpuToGPUIndices,
        std::vector<WGPUBackendDynamicShadowedDirLightData<CascadeSize>>& output,
        const glm::mat4x4& camSpaceMat,
        const std::vector<float>& cascadeRatios,
        float cascadeBleed,
        float camFar) override final {

        // Inserts non light space data into GPU data
        std::vector<glm::mat4x4> lightViews;
        lightViews.reserve(cpuType.size());
        for (int cpuIter = 0; cpuIter < cpuType.size() ; cpuIter++) {
            WGPUBackendDynamicShadowedDirLightData<CascadeSize>& outputIter = output[cpuToGPUIndices[cpuIter]];
            DirLightRenderInfo& inputIter = cpuType[cpuIter];

            outputIter.m_diffuse = inputIter.diffuse;
            outputIter.m_intensity = 1; // TODO: Implement intensity scaling
            outputIter.m_specular = inputIter.specular;
            outputIter.m_direction = GetForwardVector(&inputIter.transform);
            lightViews.push_back(GetViewMatrix(&inputIter.transform));
        }

        glm::mat4x4 invertedCamSpace = glm::inverse(camSpaceMat);

        std::array<glm::vec4, 4> nearCorners;
        std::array<glm::vec4, 4> farCorners;
        std::array<glm::vec4, 4> nearToFarCornerVectors;
        
        u8 cornerIter = 0;
        for (i8 x = -1 ; x <= 1 ; x += 2) {
            for (i8 y = -1 ; y <= 1 ; y += 2) {
                nearCorners[cornerIter] = invertedCamSpace * glm::vec4(x,y,0,1);
                nearCorners[cornerIter] /= nearCorners[cornerIter].w;

                farCorners[cornerIter] = invertedCamSpace * glm::vec4(x,y,1,1);
                farCorners[cornerIter] /= farCorners[cornerIter].w;

                nearToFarCornerVectors[cornerIter] = farCorners[cornerIter] - nearCorners[cornerIter];
                cornerIter++;
            }
        }

        // The cascade inserted should contain the same amount of ratios as the amount of cascades
        // TODO: Make non breaking on final build;
        assert(cascadeRatios.size() == CascadeSize);

        for (int cascadeIterator = 0; cascadeIterator < CascadeSize; cascadeIterator++)
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
            for (i8 x = -1 ; x <= 1 ; x += 2) {
                for (i8 y = -1 ; y <= 1 ; y += 2) {
                    corners[cornerIter] = nearCorners[wholeCornerIter] + nearToFarCornerVectors[wholeCornerIter] * trueStartRatio;
                    corners[cornerIter].w = 1;
                    cornerIter++;

                    corners[cornerIter] = nearCorners[wholeCornerIter] + nearToFarCornerVectors[wholeCornerIter] * trueEndRatio;
                    corners[cornerIter].w = 1;
                    cornerIter++;
                    wholeCornerIter++;
                }
            }

            for (int cpuIter = 0; cpuIter < cpuType.size() ; cpuIter++) {
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
                output[cpuToGPUIndices[cpuIter]].m_lightSpaces[cascadeIterator] = dirProj * lightViews[cpuIter];
            }
        }
    }
};

class WGPUBackendDynamicPointLightMap : public WGPUBackendDynamicLightVectorMap<
        WGPUBackendDynamicShadowedPointLightData,
        PointLightRenderInfo> {
protected:
    LightID GetCPULightID(const PointLightRenderInfo& cpuType) override final {
        return cpuType.lightID;
    }

    void Convert(
        std::vector<PointLightRenderInfo>& cpuType,
        std::vector<int>& cpuToGPUIndices,
        std::vector<WGPUBackendDynamicShadowedPointLightData>& output ) {
        output.reserve(cpuType.size());
        for (int cpuIter = 0; cpuIter < cpuType.size() ; cpuIter++) {
            WGPUBackendDynamicShadowedPointLightData& outputIter = output[cpuToGPUIndices[cpuIter]];
            PointLightRenderInfo& inputIter = cpuType[cpuIter];

            outputIter.m_diffuse = inputIter.diffuse;
            outputIter.m_constant = inputIter.constant; 
            outputIter.m_specular = inputIter.specular;
            outputIter.m_linear = inputIter.linear;
            outputIter.m_position = inputIter.transform.position;
            outputIter.m_quadratic = inputIter.quadratic;
        }
    } 

};

class WGPUBackendDynamicSpotLightVectorMap: public WGPUBackendDynamicLightVectorMap<
        WGPUBackendDynamicShadowedSpotLightData,
        SpotLightRenderInfo> {
protected:
    LightID GetCPULightID(const SpotLightRenderInfo& cpuType) override final {
        return cpuType.lightID;
    }

    void Convert(
        std::vector<SpotLightRenderInfo>& cpuType,
        std::vector<int>& cpuToGPUIndices,
        std::vector<WGPUBackendDynamicShadowedSpotLightData>& output ) {
        output.reserve(cpuType.size());
        for (int cpuIter = 0; cpuIter < cpuType.size() ; cpuIter++) {
            WGPUBackendDynamicShadowedSpotLightData& outputIter = output[cpuToGPUIndices[cpuIter]];
            SpotLightRenderInfo& inputIter = cpuType[cpuIter];

            outputIter.m_diffuse = inputIter.diffuse;
            outputIter.m_penumbraCutoff = inputIter.innerCone;
            outputIter.m_specular = inputIter.specular;
            outputIter.m_outerCutoff = inputIter.outerCone;
            outputIter.m_direction = GetForwardVector(&inputIter.transform);
            outputIter.m_position = inputIter.transform.position;
        }
    } 

};
