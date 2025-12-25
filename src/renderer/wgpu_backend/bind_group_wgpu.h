#pragma once

#include "math/skl_math_types.h"
#include "renderer/wgpu_backend/utils_wgpu.h"

#include <webgpu/webgpu.h>

#include <vector>
#include <algorithm>
#include <string>
#include <cassert>

// This file represents encapsulated logic of binding groups

// Encapsulates a bind group within wgpu logic
class WGPUBackendBindGroup 
{
public:
    // Encapsulates the data stored in a single uniform entry
    class IWGPUBackendUniformEntry {
    public:
        virtual WGPUBindGroupEntry GetEntry() = 0 ;
        virtual void RegisterBindGroup(WGPUBackendBindGroup& bindGroup) = 0;

        virtual ~IWGPUBackendUniformEntry() {}
    };

protected:
    // Underlying bind group
    WGPUBindGroup m_bindGroupDat;
    WGPUBindGroupLayout m_bindGroupLayout;
    // Entries to handle bind group recreations
    std::vector<std::reference_wrapper<IWGPUBackendUniformEntry>> m_bindGroupEntryDescriptors;
    // Ensures that InitOrUpdateBindGroup gets called
    bool m_inited;
    WGPUStringView m_bindGroupLabel;

public:
    // Uninitialized uniform buffer
    WGPUBackendBindGroup() { }

    void Init(const char* label, WGPUBindGroupLayout& bindLayout);
    ~WGPUBackendBindGroup();

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendBindGroup(const WGPUBackendBindGroup&) = delete;
    WGPUBackendBindGroup& operator=(const WGPUBackendBindGroup&) = delete;
    WGPUBackendBindGroup(WGPUBackendBindGroup&&) = delete;
    WGPUBackendBindGroup& operator=(WGPUBackendBindGroup&&) = delete;

    // Uses current entries to recreate binding groups
    // Needs to be called once before initialization
    void InitOrUpdateBindGroup(const WGPUDevice& device);

    // Applies bind group to render pass
    // Needs to be inited beforehand
    void BindToRenderPass(WGPURenderPassEncoder& renderPass);

    // Inserts a uniform entry into a binding group
    void AddEntryToBindingGroup(IWGPUBackendUniformEntry& entry);
};

template <typename BufferStruct>
class WGPUBackendArrayBuffer {
private:
    // A descriptor meant to assist in resizing
    WGPUBufferDescriptor m_bufferDescriptor;

    // The amount of the given struct that should be allowed in the struct
    u32 m_bufferLimit;
protected:
    // The amount of struct sizes allocated within the buffer
    u32 m_currentBufferAllocatedSize;
    // The amount of struct space actually being used by the buffer
    u32 m_currentBufferSize;
    // The encapsulated buffer
    WGPUBuffer m_bufferDat; 

    // Recreates buffer data to new size 
    void ResizeTo(const WGPUDevice& device, const WGPUQueue& queue, u32 newStructDataSize) {
        // Creates resized buffer that is at least contains one struct (even if totally empty)
        if (newStructDataSize == 0) {
            newStructDataSize = 1;
        }
        m_bufferDescriptor.size = newStructDataSize * sizeof(BufferStruct);
        WGPUBuffer tempBuffer = wgpuDeviceCreateBuffer(device, &m_bufferDescriptor);

        // Moves old data into new resized data and destroys old buffer
        WGPUCommandEncoderDescriptor reallocateBufferCommandDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("WGPUBackendSingleUniformBuffer resizing operation command encoder")
        };
        WGPUCommandEncoder reallocateBufferCommand = wgpuDeviceCreateCommandEncoder(device, &reallocateBufferCommandDesc);
        wgpuCommandEncoderCopyBufferToBuffer(reallocateBufferCommand, m_bufferDat, 0, tempBuffer, 0, m_currentBufferAllocatedSize * sizeof(BufferStruct));
        WGPUCommandBufferDescriptor reallocateBufferCommandBufferDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("WGPUBackendSingleUniformBuffer resizing operation command buffer")
        };
        WGPUCommandBuffer reallocateBufferCommandBuffer = wgpuCommandEncoderFinish(reallocateBufferCommand, &reallocateBufferCommandBufferDesc);
        wgpuCommandEncoderRelease(reallocateBufferCommand);
        wgpuQueueSubmit(queue, 1, &reallocateBufferCommandBuffer);
        wgpuCommandBufferRelease(reallocateBufferCommandBuffer);

        // Replaces old data with new resized data
        wgpuBufferDestroy(m_bufferDat);
        m_bufferDat = tempBuffer;

        // Actually updates stored count
        m_currentBufferAllocatedSize = newStructDataSize;
    }

public:
    // Uninitialized uniform buffer
    WGPUBackendArrayBuffer() { }

    void Init(const WGPUDevice& device, WGPUBufferUsage additionalUsage, const char* bufferLabel, u32 sizeLimit) {
        m_bufferLimit = sizeLimit;
        m_currentBufferAllocatedSize = 0;
        m_currentBufferSize = 0;

        m_bufferDescriptor = WGPUBufferDescriptor {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr(bufferLabel),
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc | additionalUsage,
            .size = sizeof(BufferStruct),
            .mappedAtCreation = false
        };

        m_bufferDat = wgpuDeviceCreateBuffer(device, &m_bufferDescriptor);
    }

    virtual ~WGPUBackendArrayBuffer() {
        if (m_bufferDat != nullptr) {
            wgpuBufferDestroy(m_bufferDat);
        }
    }

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendArrayBuffer(const WGPUBackendArrayBuffer&) = delete;
    WGPUBackendArrayBuffer& operator=(const WGPUBackendArrayBuffer&) = delete;
    WGPUBackendArrayBuffer(WGPUBackendArrayBuffer&&) = delete;
    WGPUBackendArrayBuffer& operator=(WGPUBackendArrayBuffer&&) = delete;

    // Removes a range from the vertex (not byte size index but rather struct vector idx)
    void EraseRange(WGPUDevice& device, WGPUQueue& queue, u32 beginIdx, u32 count) {
        // Ensures that range is valid
        assert(m_currentBufferSize >= count);
        WGPUCommandEncoderDescriptor eraseCommandDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("Buffer erase range command")
        };
        WGPUCommandEncoder eraseCommand = wgpuDeviceCreateCommandEncoder(device, &eraseCommandDesc);

        wgpuCommandEncoderCopyBufferToBuffer(
            eraseCommand, 
            m_bufferDat,  
            (beginIdx + count) * sizeof(BufferStruct), 
            m_bufferDat, 
            beginIdx * sizeof(BufferStruct), 
            count * sizeof(BufferStruct));
        
        WGPUCommandBufferDescriptor eraseCommandBufferDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("Buffer erase range command buffer")
        };
        WGPUCommandBuffer eraseCommandBuffer = wgpuCommandEncoderFinish(eraseCommand, &eraseCommandBufferDesc);
        wgpuCommandEncoderRelease(eraseCommand);

        wgpuQueueSubmit(queue, 1, &eraseCommandBuffer);
        wgpuCommandBufferRelease(eraseCommandBuffer);
    }

    // Appends data to the end of the buffer
    // Will resize existing buffer if too small by doubling size until big enough
    // Will update bind groups accordingly
    void AppendToBack(const WGPUDevice& device, const WGPUQueue& queue, const std::vector<BufferStruct>& data) {
        AppendToBack(device, queue, data.data(), data.size());
    }
    virtual void AppendToBack(const WGPUDevice& device, const WGPUQueue& queue, const BufferStruct* data, u32 datLength) {
        if(datLength + m_currentBufferSize > m_currentBufferAllocatedSize) {
            u32 newBufferSize = m_currentBufferAllocatedSize;
            
            if (newBufferSize == 0) {
                newBufferSize = 1;
            }
            while (newBufferSize < datLength + m_currentBufferSize) {
                newBufferSize *= 2;
                if(newBufferSize > m_bufferLimit) {
                    // TODO: Make sure this only happens in debug mode/non game mode
                    assert(datLength + m_currentBufferSize < m_bufferLimit);
                    newBufferSize = m_bufferLimit;
                    break;
                }
            }

            ResizeTo(device, queue, newBufferSize);
        }

        // Actually writes to queue
        wgpuQueueWriteBuffer(queue, m_bufferDat, m_currentBufferSize * sizeof(BufferStruct), data, datLength * sizeof(BufferStruct));
        m_currentBufferSize += datLength; 

    }

    // Directly writes to the uniform buffer using the given data
    // Will double buffer size until data fits.
    // Does nothing on 0 buffer length set.
    // If the data entered is smaller than previous allocated size, remaining data after inserted data should be ignored.
    void WriteBuffer(const WGPUDevice& device, const WGPUQueue& queue, const std::vector<BufferStruct>& data) {
        WriteBuffer(queue, device, data.data(), data.size());
    }
    virtual void WriteBuffer(const WGPUDevice& device, const WGPUQueue& queue, const BufferStruct* data, u32 datLength) {
        // Resizes all relevant bind groups to fit in
        if(datLength > m_currentBufferAllocatedSize) {
            u32 newBufferSize = m_currentBufferAllocatedSize;
            
            if (newBufferSize == 0) {
                newBufferSize = 1;
            }
            while (newBufferSize < datLength) {
                newBufferSize *= 2;
                if(newBufferSize > m_bufferLimit) {
                    // TODO: Make sure this only happens in debug mode/non game mode
                    assert(datLength < m_bufferLimit);
                    newBufferSize = m_bufferLimit;
                    break;
                }
            }

            ResizeTo(device, queue, newBufferSize);
        }
        
        // Actually writes to queue
        wgpuQueueWriteBuffer(queue, m_bufferDat, 0, data, datLength * sizeof(BufferStruct));
        m_currentBufferSize = datLength;
    }

    void BindToRenderPassAsVertexBuffer(WGPURenderPassEncoder& encoder) {
        wgpuRenderPassEncoderSetVertexBuffer(encoder, 0, m_bufferDat, 0, m_currentBufferSize * sizeof(BufferStruct));
    }

    void BindToRenderPassAsIndexBuffer(WGPURenderPassEncoder& encoder) {
        wgpuRenderPassEncoderSetIndexBuffer(encoder, m_bufferDat, WGPUIndexFormat_Uint32, 0, m_currentBufferSize * sizeof(BufferStruct));
    }
};

// Encapsulates a buffer that holding a array/vector of structs to be put into a single 
// binded storage entry and dynamically changes buffer size based inputted data.
template <typename StorageStruct>
class WGPUBackendSingleStorageArrayBuffer : public WGPUBackendArrayBuffer<StorageStruct>, public WGPUBackendBindGroup::IWGPUBackendUniformEntry {
private:
    std::vector<std::reference_wrapper<WGPUBackendBindGroup>> m_bindGroups{ };
    WGPUBindGroupEntry m_currentBindGroupEntry{ };

    void UpdateRegisteredBindGroups(const WGPUDevice& device, const WGPUQueue& queue) {
        // Updates command buffer entry accordingly
        m_currentBindGroupEntry.buffer = WGPUBackendArrayBuffer<StorageStruct>::m_bufferDat;
        m_currentBindGroupEntry.size = WGPUBackendArrayBuffer<StorageStruct>::m_currentBufferAllocatedSize * sizeof(StorageStruct);

        // Recreates binding group
        for (std::reference_wrapper<WGPUBackendBindGroup> bindGroup : m_bindGroups) {
            bindGroup.get().InitOrUpdateBindGroup(device);
        }
    }

protected:
    WGPUBindGroupEntry GetEntry() override {
        return m_currentBindGroupEntry;
    }

    void RegisterBindGroup(WGPUBackendBindGroup& bindGroup) override {
        m_bindGroups.push_back(bindGroup);
    }

public:
    // Will update bind groups accordingly
    void AppendToBack(const WGPUDevice& device, const WGPUQueue& queue, const StorageStruct* data, u32 datLength) override {
        WGPUBackendArrayBuffer<StorageStruct>::AppendToBack(device, queue, data, datLength);
        UpdateRegisteredBindGroups(device, queue);
    }

    // Will resize existing buffer if too small to the size of the data written and will update bind groups accordingly
    // Will simply do nothing on zero buffer size
    void WriteBuffer(const WGPUDevice& device, const WGPUQueue& queue, const StorageStruct* data, u32 datLength) override {
        if (datLength == 0) {
            return;
        }
        WGPUBackendArrayBuffer<StorageStruct>::WriteBuffer(device, queue, data, datLength);
        UpdateRegisteredBindGroups(device, queue);
    }

    // Uninitialized array buffer
    WGPUBackendSingleStorageArrayBuffer() { }

    // For now begin size and size limits describe amount of given struct that can be, not the byte size
    void Init(const WGPUDevice& device, const char* bufferLabel, u32 binding, u32 sizeLimit) {
        WGPUBackendArrayBuffer<StorageStruct>::Init(device, WGPUBufferUsage_Storage, bufferLabel, sizeLimit);
        m_currentBindGroupEntry = WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = binding,
            .buffer = WGPUBackendArrayBuffer<StorageStruct>::m_bufferDat,
            .offset = 0,
            .size = sizeof(StorageStruct)
        };
    }

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendSingleStorageArrayBuffer(const WGPUBackendSingleStorageArrayBuffer&) = delete;
    WGPUBackendSingleStorageArrayBuffer& operator=(const WGPUBackendSingleStorageArrayBuffer&) = delete;
    WGPUBackendSingleStorageArrayBuffer(WGPUBackendSingleStorageArrayBuffer&&) = delete;
    WGPUBackendSingleStorageArrayBuffer& operator=(WGPUBackendSingleStorageArrayBuffer&&) = delete;
};

// Represents a buffer holding a single struct
// to be put into a single binded uniform entry
// Does not ever update bind groups
template <typename UniformStruct>
class WGPUBackendSingleUniformBuffer : public WGPUBackendBindGroup::IWGPUBackendUniformEntry {
private:
    // The encapsulated uniform buffer (Will not really ever change)
    WGPUBuffer m_bufferDat;
    WGPUBindGroupEntry m_currentBindGroupEntry;

    WGPUBindGroupEntry GetEntry() override {
        return m_currentBindGroupEntry;
    }

    // No need to update bind group therefore no change
    void RegisterBindGroup(WGPUBackendBindGroup& bindGroup) override { }

public:
    // Uninitialized uniform buffer
    WGPUBackendSingleUniformBuffer() { }

    void Init(const WGPUDevice& device, const char* bufferLabel, u32 binding) {
        WGPUBufferDescriptor bufferDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr(bufferLabel),
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = sizeof(UniformStruct),
            .mappedAtCreation = false
        };

        m_bufferDat = wgpuDeviceCreateBuffer(device, &bufferDesc);

        m_currentBindGroupEntry = WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = binding,
            .buffer = m_bufferDat,
            .offset = 0,
            .size = sizeof(UniformStruct)
        };
    }

    ~WGPUBackendSingleUniformBuffer() {
        if (m_bufferDat != nullptr) {
            wgpuBufferRelease(m_bufferDat);
        }
    }

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendSingleUniformBuffer(const WGPUBackendSingleUniformBuffer&) = delete;
    WGPUBackendSingleUniformBuffer& operator=(const WGPUBackendSingleUniformBuffer&) = delete;
    WGPUBackendSingleUniformBuffer(WGPUBackendSingleUniformBuffer&&) = delete;
    WGPUBackendSingleUniformBuffer& operator=(WGPUBackendSingleUniformBuffer&&) = delete;

    void WriteBuffer(WGPUQueue& queue, const UniformStruct& data) {
        wgpuQueueWriteBuffer(queue, m_bufferDat, 0, &data, sizeof(UniformStruct));
    }
};

// Sampler
class WGPUBackendSampler : public WGPUBackendBindGroup::IWGPUBackendUniformEntry {
private:
    // Encapsulated sampler data
    WGPUSampler m_samplerData = nullptr;
    WGPUBindGroupEntry m_currentBindGroupEntry;
    std::vector<std::reference_wrapper<WGPUBackendBindGroup>> m_bindGroups;
    bool m_inited = false;

    void UpdateRegisteredBindGroups(const WGPUDevice& device, const WGPUQueue& queue) {
        // Recreates binding group
        for (std::reference_wrapper<WGPUBackendBindGroup> bindGroup : m_bindGroups) {
            bindGroup.get().InitOrUpdateBindGroup(device);
        }
    }

    // No need to update bind group therefore no change
    void RegisterBindGroup(WGPUBackendBindGroup& bindGroup) override { m_bindGroups.push_back(bindGroup); }

    WGPUBindGroupEntry GetEntry() override {
        return m_currentBindGroupEntry;
    }

public:
    // Uninitialized uniform buffer
    WGPUBackendSampler() { }

    ~WGPUBackendSampler() {
        if (m_samplerData != nullptr) {
            wgpuSamplerRelease(m_samplerData);
        }
    }
    void InitOrUpdate(
        const WGPUDevice& device,
        const WGPUQueue& queue,
        WGPUAddressMode expandBehavior, 
        WGPUFilterMode magFilter, 
        WGPUFilterMode minFilter, 
        WGPUMipmapFilterMode mipmapFilter, 
        float lowestMipMap,
        float greatestMipMap,
        WGPUCompareFunction compareFunction,
        u16 maxAnisotropy,
        const std::string& label,
        u32 binding
        ) {
        WGPUSamplerDescriptor samplerDesc {
            .nextInChain = nullptr,
            .label = label.data(),
            .addressModeU = expandBehavior,
            .addressModeV = expandBehavior,
            .addressModeW = expandBehavior,
            .magFilter = magFilter,
            .minFilter = minFilter,
            .mipmapFilter = mipmapFilter,
            .lodMinClamp = lowestMipMap,
            .lodMaxClamp = greatestMipMap,
            .compare = compareFunction,
            .maxAnisotropy = maxAnisotropy,
        };
    
        m_samplerData = wgpuDeviceCreateSampler(device, &samplerDesc);

        m_currentBindGroupEntry = {
            .binding = binding,
            .sampler = m_samplerData
        };

        m_inited = true;
        UpdateRegisteredBindGroups(device, queue);
    }

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendSampler(const WGPUBackendSampler&) = delete;
    WGPUBackendSampler& operator=(const WGPUBackendSampler&) = delete;
    WGPUBackendSampler(WGPUBackendSampler&&) = delete;
    WGPUBackendSampler& operator=(WGPUBackendSampler&&) = delete;
};
