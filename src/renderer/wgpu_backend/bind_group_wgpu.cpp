#include "renderer/wgpu_backend/bind_group_wgpu.h"

void WGPUBackendBindGroup::Init(const char* label, WGPUBindGroupLayout& bindLayout) {
    m_bindGroupLayout = bindLayout;
    m_inited = false;
    m_bindGroupLabel = WGPUBackendUtils::wgpuStr(label);
    wgpuBindGroupLayoutAddRef(m_bindGroupLayout);
}

WGPUBackendBindGroup::~WGPUBackendBindGroup() {
    if (m_bindGroupLayout != nullptr) {
        wgpuBindGroupLayoutRelease(m_bindGroupLayout);
    }
    if (m_bindGroupDat != nullptr) {
        wgpuBindGroupRelease(m_bindGroupDat);
    }
}

void WGPUBackendBindGroup::InitOrUpdateBindGroup(const WGPUDevice& device) {
    m_inited = true;

    std::vector<WGPUBindGroupEntry> bindGroups;
    for (IWGPUBackendUniformEntry& entry : m_bindGroupEntryDescriptors) {
        bindGroups.push_back(entry.GetEntry());
    }

    WGPUBindGroupDescriptor bindGroupDesc {
        .nextInChain = nullptr,
        .label = m_bindGroupLabel,
        .layout = m_bindGroupLayout,
        .entryCount = bindGroups.size(),
        .entries = bindGroups.data()
    };

    m_bindGroupDat = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}

void WGPUBackendBindGroup::BindToRenderPass(WGPURenderPassEncoder& renderPass) {
    if(!m_inited) {
        // TODO: Make sure assert is only hit in debug
        assert(true);
        return;
    }

    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroupDat, 0, nullptr);
}

void WGPUBackendBindGroup::AddEntryToBindingGroup(IWGPUBackendUniformEntry& entry) {
    m_bindGroupEntryDescriptors.push_back(std::reference_wrapper<IWGPUBackendUniformEntry>(entry));
    entry.RegisterBindGroup(*this);
}
