#pragma once

#include <skl_math_types.h>
#include <utils_wgpu.h>
#include <render_types_wgpu.h>
#include <bind_group_wgpu.h>

#include <webgpu/webgpu.h>

#include <vector>

// Represents a singular texture within the program. 
class WebGPUBackendCubemapTextureBuffer : public WGPUBackendBindGroup::IWGPUBackendUniformEntry {
private:
    WGPUBindGroupEntry m_currentBindGroupEntry;
    std::vector<WGPUBackendBindGroup*> m_bindGroups;
    std::string m_label;
    std::string m_viewLabel;
    WGPUTexture m_textureData;
    WGPUTextureView m_textureCubemapView;
    u32 m_width;
    u32 m_height;
    bool m_inited;

    void ClearBuffers();
    
    void UpdateBindGroups(const WGPUDevice& device);

public:
    WebGPUBackendCubemapTextureBuffer();
    virtual ~WebGPUBackendCubemapTextureBuffer();

    bool GetInitiated();

    // Directly takes in cubemap data
    void Init(
        const WGPUDevice& device, 
        u32 texturesWidth, 
        u32 texturesHeight, 
        std::string&& label,
        std::string&& viewLabel);

    void Reinit(
        const WGPUDevice& device, 
        u32 texturesWidth, 
        u32 texturesHeight, 
        std::string&& label,
        std::string&& viewLabel);


    void Insert(
        const WGPUDevice& device, 
        const WGPUQueue& queue,
        u32 texturesWidth, 
        u32 texturesHeight, 
        std::array<u32*,6> faceData);

    WGPUTextureView getView();
    
    // Used to update bind group on underlying texture change
    WGPUBindGroupEntry GetEntry(u32 binding) override;
    void RegisterBindGroup(WGPUBackendBindGroup* bindGroup) override;

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WebGPUBackendCubemapTextureBuffer(const WebGPUBackendCubemapTextureBuffer&) = delete;
    WebGPUBackendCubemapTextureBuffer& operator=(const WebGPUBackendCubemapTextureBuffer&) = delete;
    WebGPUBackendCubemapTextureBuffer(WebGPUBackendCubemapTextureBuffer&&) = delete;
    WebGPUBackendCubemapTextureBuffer& operator=(WebGPUBackendCubemapTextureBuffer&&) = delete;
};
