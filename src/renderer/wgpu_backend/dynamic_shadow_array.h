#pragma once 

#include "math/skl_math_types.h"
#include "renderer/wgpu_backend/utils_wgpu.h"
#include "renderer/wgpu_backend/render_types_wgpu.h"
#include "renderer/wgpu_backend/bind_group_wgpu.h"

#include <webgpu/webgpu.h>

#include <string>
#include <vector>

// Encapsulates a single Shadow map texture array
class WGPUBackendBaseDynamicShadowMapArray : public WGPUBackendBindGroup::IWGPUBackendUniformEntry {
private:
    WGPUBindGroupEntry m_currentBindGroupEntry;
    std::vector<std::reference_wrapper<WGPUBackendBindGroup>> m_bindGroups;
    std::vector<WGPUTextureView> m_arrayLayerViews;
    WGPUTextureView m_wholeTextureDataView;
    WGPUTexture m_textureData;
    std::string m_label;
    std::string m_wholeViewLabel;
    std::string m_layerViewLabel;
    WGPUTextureViewDimension m_wholeTextureViewDimension;
    u32 m_arrayLayerWidth;
    u32 m_arrayLayerHeight;
    u16 m_arraySize;
    u16 m_arrayAllocatedSize;
    u16 m_arrayMaxAllocatedSize;
    u16 m_depthPerShadow;
    bool m_inited;

protected:
    void UpdateAttachedBindGroups(const WGPUDevice& device);

    u16 GenerateNewAllocatedSize (u16 newArraySize);

    // Continually doubles allocated size until allocation can fit given new arraySize.
    // Doesn't actually edit arraySize however.
    void ResizeTexture(const WGPUDevice& device, const WGPUQueue& queue, u16 newArraySize);
    // Deallocates all memory on the WGPUSide that the shadow map contains
    // and resets to pre-init state
    void Clear();

public:
    // Newly constructed shadow maps needs to need to be inited before use.
    WGPUBackendBaseDynamicShadowMapArray();

    virtual ~WGPUBackendBaseDynamicShadowMapArray();

    void Init(
        const WGPUDevice& device, 
        u32 arrayLayerWidth, 
        u32 arrayLayerHeight, 
        u16 maxTextureDepth, 
        u16 depthPerShadow, 
        std::string label,
        std::string wholeViewLabel,
        std::string layerViewLabel, 
        u16 binding,
        bool cubeMapView);
    // Completely clears all previous data and remakes shadow map
    void Reset(
        const WGPUDevice& device, 
        u32 arrayLayerWidth, 
        u32 arrayLayerHeight, 
        u16 maxTextureDepth, 
        u16 depthPerShadow, 
        std::string label,
        std::string wholeViewLabel,
        std::string layerViewLabel,
        u16 binding,
        bool cubeMapView);

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendBaseDynamicShadowMapArray(const WGPUBackendBaseDynamicShadowMapArray&) = delete;
    WGPUBackendBaseDynamicShadowMapArray& operator=(const WGPUBackendBaseDynamicShadowMapArray&) = delete;
    WGPUBackendBaseDynamicShadowMapArray(WGPUBackendBaseDynamicShadowMapArray&&) = delete;
    WGPUBackendBaseDynamicShadowMapArray& operator=(WGPUBackendBaseDynamicShadowMapArray&&) = delete;

    // Ensures that there is enough space for shadow to edit
    void RegisterShadow(const WGPUDevice& device, const WGPUQueue& queue);
    // Removes selected shadow from array
    void UnregisterShadow();

    // Used to update bind group on shadow texture resizing
    WGPUBindGroupEntry GetEntry() override;
    void RegisterBindGroup(WGPUBackendBindGroup& bindGroup) override;

    WGPUTextureView GetView(u16 shadowIndex);
};
