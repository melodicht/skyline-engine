#pragma once

#include "math/skl_math_consts.h"
#include "renderer/wgpu_backend/utils_wgpu.h"
#include "renderer/wgpu_backend/render_types_wgpu.h"
#include "renderer/wgpu_backend/bind_group_wgpu.h"

#include <webgpu/webgpu.h>

#include <vector>

// Represents a singular texture within the program. 
class WebGPUBackendCubemapTextureBuffer : public WGPUBackendBindGroup::IWGPUBackendUniformEntry {
private:
    WGPUBindGroupEntry m_currentBindGroupEntry;
    std::vector<std::reference_wrapper<WGPUBackendBindGroup>> bindGroups;
    WGPUTextureView m_wholeTextureDataView;
    WGPUTexture m_textureBuffer;

    WebGPUBackendCubemapTextureBuffer();

    void ClearBuffers();

    void UpdateBindGroups();

public:
    virtual ~WebGPUBackendCubemapTextureBuffer();

    // Directly takes in cubemap data
    // TODO: Ideally the renderer should not be handling this logic so directly, find a more encapsulated way of dealing with such data
    void Init(
        const WGPUDevice& device, 
        u32 arrayLayerWidth, 
        u32 arrayLayerHeight, 
        u16 maxTextureDepth, 
        std::string&& label,
        std::string&& wholeViewLabel,
        std::string&& layerViewLabel, 
        std::string&& frontPath,
        std::string&& leftPath,
        std::string&& rightPath,
        std::string&& upPath,
        std::string&& downPath,
        std::string&& backPath,
        u16 binding);

    void Reinit(
        const WGPUDevice& device, 
        u32 arrayLayerWidth, 
        u32 arrayLayerHeight, 
        u16 maxTextureDepth, 
        std::string&& label,
        std::string&& wholeViewLabel,
        std::string&& layerViewLabel, 
        std::string&& frontPath,
        std::string&& leftPath,
        std::string&& rightPath,
        std::string&& upPath,
        std::string&& downPath,
        std::string&& backPath,
        u16 binding);

    WGPUTextureView getView();
    
    // Used to update bind group on underlying texture change
    WGPUBindGroupEntry GetEntry() override;
    void RegisterBindGroup(WGPUBackendBindGroup& bindGroup) override;

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WebGPUBackendCubemapTextureBuffer(const WebGPUBackendCubemapTextureBuffer&) = delete;
    WebGPUBackendCubemapTextureBuffer& operator=(const WebGPUBackendCubemapTextureBuffer&) = delete;
    WebGPUBackendCubemapTextureBuffer(WebGPUBackendCubemapTextureBuffer&&) = delete;
    WebGPUBackendCubemapTextureBuffer& operator=(WebGPUBackendCubemapTextureBuffer&&) = delete;
};