#pragma once

#include <skl_math_types.h>
#include <utils_wgpu.h>

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
    class WGPUBackendBindGroupEntry {
    protected: 
        WGPUBindGroupEntry m_currentBindGroupEntry;    
        
    public:
        WGPUBindGroupEntry GetEntry(u32 binding) const;

        virtual void RegisterBindGroup(WGPUBackendBindGroup* bindGroup, u32 binding);

        WGPUBackendBindGroupEntry() = default;
        virtual ~WGPUBackendBindGroupEntry() = default;
    };

    class DirtyMarkingBindGroupEntry : public WGPUBackendBindGroup::WGPUBackendBindGroupEntry{
    protected:
        std::vector<WGPUBackendBindGroup*> m_registeredBindGroups;

        // Marks all registered bind groups as dirty
        void DirtyBindingGroups();
    public:

        // Ensures that registered bind groups are listed such that they can be marked as dirty later
        virtual void RegisterBindGroup(WGPUBackendBindGroup* bindGroup, u32 binding) override;
        
    };


private:
    // Inserts a uniform entry into a binding group
    void AddEntryToBindingGroup(const WGPUBackendBindGroupEntry* entry, u32 binding);

    void DirtyBindGroup();
protected:
    // Underlying bind group
    WGPUBindGroup m_bindGroupDat{ nullptr };
    WGPUBindGroupLayout m_bindGroupLayout{ nullptr };
    // Entries to handle bind group recreations
    struct WGPUBackendBindedEntry {
        const WGPUBackendBindGroupEntry* m_entry { nullptr };
        u32 m_entryBind{ 0 };
    };
    std::vector<WGPUBackendBindedEntry> m_bindGroupEntries{ };
    WGPUStringView m_bindGroupLabel{ nullptr, 0};
    // Ensures that UpdateBindGroup gets called once
    b8 m_dirty{ true };

    virtual std::vector<WGPUBindGroupEntry> GetEntries() const;

public:
    // Uninitialized uniform buffer
    WGPUBackendBindGroup() = default;

    void Init(const char* label, const WGPUBindGroupLayout& bindLayout, u32 dynamicStride);
    ~WGPUBackendBindGroup();

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendBindGroup(const WGPUBackendBindGroup&) = delete;
    WGPUBackendBindGroup& operator=(const WGPUBackendBindGroup&) = delete;
    WGPUBackendBindGroup(WGPUBackendBindGroup&&) = delete;
    WGPUBackendBindGroup& operator=(WGPUBackendBindGroup&&) = delete;

    // Uses current entries to recreate binding groups
    // Needs to be called once before initialization
    // Only gets executes logic if dirty.
    void UpdateBindGroup(const WGPUDevice& device);

    // Applies bind group to render pass
    // Needs to be inited beforehand 
    // Assumes that it isn't dirty
    void BindToRenderPass(u32 groupIdx, const WGPURenderPassEncoder& renderPass) const;
};

// Entry that Dynamic Bind Groups take in to get dynamic uniforms from
class WGPUBackendDynamicUniformEntry {
protected:
    WGPUBindGroupEntry m_currentDynamicUniformEntry;  

public:
    WGPUBackendDynamicUniformEntry() = default;

    WGPUBindGroupEntry GetDynamicUniformEntry(u32 binding) const;
};

// Encapsulates a bind group with dynamic uniform buffers
template<u32 DynamicEntryCount>
class WGPUBackendDynamicBindGroup : public WGPUBackendBindGroup {
private:
    struct WGPUBackendBindedDynamicEntry {
        const WGPUBackendDynamicUniformEntry* m_uniformEntry;
        u32 m_entryBind;
        u32 m_dynamicStride;
    };

    std::array<WGPUBackendBindedDynamicEntry, DynamicEntryCount> m_dynamicUniformEntries{ };
    u32 m_filledEntryCount = 0;

    void AddDynamicUniformEntryToBindingGroup(const WGPUBackendDynamicUniformEntry* entry, u32 entryDynamicStride, u32 binding) {
        m_dynamicUniformEntries[m_filledEntryCount] = {entry, binding, entryDynamicStride};
        m_filledEntryCount++;
    }

    virtual std::vector<WGPUBindGroupEntry> GetEntries() const override {
        // Gets the entry descriptors of all non dynamic entries
        std::vector<WGPUBindGroupEntry> bindGroupEntryDescriptors = WGPUBackendBindGroup::GetEntries();

        // Gets the entry for dynamic uniform entries
        ASSERT_PRINT(m_filledEntryCount == DynamicEntryCount, "Dynamic Uniform Entries Not Completely Filled Yet");
        for (int i = 0 ; i < m_filledEntryCount ; i++) {
            WGPUBackendBindedDynamicEntry entry = m_dynamicUniformEntries[i];
            bindGroupEntryDescriptors.push_back(entry.m_uniformEntry->GetDynamicUniformEntry(entry.m_entryBind));
        }
        return bindGroupEntryDescriptors;
    }

    friend class WGPUBackendDynamicUniformStore;

public:

    // Inserts a uniform entry into a binding group
    // dynamic index goes in order that  
    void BindToRenderPass(u32 groupIdx, const WGPURenderPassEncoder& renderPass, std::array<u32, DynamicEntryCount> dynamicIndex) const {
        ASSERT(!m_dirty);
        
        for (int i = 0 ; i < DynamicEntryCount ; i++) {
            dynamicIndex[i] *= m_dynamicUniformEntries[i].m_dynamicStride;
        }
        wgpuRenderPassEncoderSetBindGroup(renderPass, groupIdx, m_bindGroupDat, DynamicEntryCount, dynamicIndex.data());
    }

};

// Base class meant to allow for the actual registering of bind groups into dynamic uniform.
// Assumes any dynamic uniform buffers are adjustable and can be dirty. 
class WGPUBackendDynamicUniformStore : public WGPUBackendDynamicUniformEntry, public virtual WGPUBackendBindGroup::DirtyMarkingBindGroupEntry {
protected:
    u32 m_dynamicStride{ 0 };

public:
    WGPUBackendDynamicUniformStore() = default;

    // Helper that exposes bind group logic to entries
    template<u32 DynamicEntryCount>
    void RegisterBindGroupAsDynamicUniform(WGPUBackendDynamicBindGroup<DynamicEntryCount>* group, u32 binding) {
        group->AddDynamicUniformEntryToBindingGroup(this, m_dynamicStride, binding);
        this->m_registeredBindGroups.push_back(static_cast<WGPUBackendBindGroup*>(group));
    }

    virtual ~WGPUBackendDynamicUniformStore() = default;
};

// Abstracts away much of the logic required to hold an gpu that
// holds a vector of the same type of data
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
    // Stride of a single entry in the array buffer
    u32 m_bufferSlotStride;
    // The encapsulated buffer
    WGPUBuffer m_bufferDat; 

    // Finds a new size for the array buffer
    u32 CalculateNewStructSize(u32 requiredStructsLength) {
        u32 newBufferSize = m_currentBufferAllocatedSize;
        if (newBufferSize == 0) {
            newBufferSize = 1;
        }
        while (newBufferSize < requiredStructsLength) {
            newBufferSize *= 2;
            if(newBufferSize > m_bufferLimit) {
                ASSERT(requiredStructsLength < m_bufferLimit);
                newBufferSize = m_bufferLimit;
                break;
            }
        }

        return newBufferSize;
    }

    void InitToSize(const WGPUDevice& device, WGPUBufferUsage additionalUsage, const char* bufferLabel, u32 sizeLimit, u32 size) {
        m_bufferLimit = sizeLimit;
        m_currentBufferAllocatedSize = 0;
        m_currentBufferSize = 0;

        m_bufferDescriptor = WGPUBufferDescriptor {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr(bufferLabel),
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc | additionalUsage,
            .size = size,
            .mappedAtCreation = false
        };

        m_bufferDat = wgpuDeviceCreateBuffer(device, &m_bufferDescriptor);
        m_bufferSlotStride = sizeof(BufferStruct);
    }

    void Init(const WGPUDevice& device, WGPUBufferUsage additionalUsage, const char* bufferLabel, u32 sizeLimit) {
        InitToSize(device, additionalUsage, bufferLabel, sizeLimit, sizeof(BufferStruct));
    }
    
    // Resets buffer data to new size and preserves data in buffer in new buffer
    virtual void ResizeTo(const WGPUDevice& device, const WGPUQueue& queue, u32 newStructsize) {
        // Creates resized buffer that is at least contains one struct (even if totally empty)
        if (newStructsize == 0) {
            newStructsize = 1;
        }
        m_bufferDescriptor.size = newStructsize * m_bufferSlotStride;
        WGPUBuffer tempBuffer = wgpuDeviceCreateBuffer(device, &m_bufferDescriptor);

        // Moves old data into new resized data and destroys old buffer
        WGPUCommandEncoderDescriptor reallocateBufferCommandDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("WGPUBackendSingleUniformBuffer resizing operation command encoder")
        };
        WGPUCommandEncoder reallocateBufferCommand = wgpuDeviceCreateCommandEncoder(device, &reallocateBufferCommandDesc);
        wgpuCommandEncoderCopyBufferToBuffer(reallocateBufferCommand, m_bufferDat, 0, tempBuffer, 0, m_currentBufferAllocatedSize * m_bufferSlotStride);
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
        m_currentBufferAllocatedSize = newStructsize;
    }

    // Resizes buffer to given size however completely clears information inside buffer
    virtual void ResizeToTransient(const WGPUDevice& device, u32 newStructsize) {
        // Creates resized buffer that is at least contains one struct (even if totally empty)
        if (newStructsize == 0) {
            newStructsize = 1;
        }
        m_bufferDescriptor.size = newStructsize * m_bufferSlotStride;
        WGPUBuffer tempBuffer = wgpuDeviceCreateBuffer(device, &m_bufferDescriptor);

        // Replaces old buffer with new resized buffer
        wgpuBufferDestroy(m_bufferDat);
        m_bufferDat = tempBuffer;

        // Actually updates stored count
        m_currentBufferAllocatedSize = newStructsize;
    }
public:
    // Uninitialized uniform buffer
    WGPUBackendArrayBuffer() = default;

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
        ASSERT(m_currentBufferSize >= count);
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
    virtual void AppendToBack(const WGPUDevice& device, const WGPUQueue& queue, const BufferStruct* data, u32 structLength) {
        // Checks if allocated data has enough space to fit in data.
        // Accordingly adjusts size.
        if(structLength + m_currentBufferSize > m_currentBufferAllocatedSize) {
            ResizeTo(device, queue, CalculateNewStructSize(structLength + m_currentBufferSize));
        }

        // Actually writes to queue
        wgpuQueueWriteBuffer(queue, m_bufferDat, m_currentBufferSize * sizeof(BufferStruct), data, structLength * sizeof(BufferStruct));
        m_currentBufferSize += structLength; 

    }

    // Directly writes to the uniform buffer using the given data
    // Will double buffer size until data fits.
    // Does nothing on 0 buffer length set.
    // If the data entered is smaller than previous allocated size, remaining data after inserted data should be ignored.
    void WriteBuffer(const WGPUDevice& device, const WGPUQueue& queue, const std::vector<BufferStruct>& data) {
        WriteBuffer(device, queue, data.data(), data.size());
    }
    virtual void WriteBuffer(const WGPUDevice& device, const WGPUQueue& queue, const BufferStruct* data, u32 structLength) {
        if (structLength == 0) {
            return;
        }
        // Ensures that buffer can take in given data
        if(structLength > m_currentBufferAllocatedSize) {
            ResizeToTransient(device, CalculateNewStructSize(structLength));
        }
        
        // Actually writes to queue
        wgpuQueueWriteBuffer(queue, m_bufferDat, 0, data, structLength * sizeof(BufferStruct));
        m_currentBufferSize = structLength;
    }
};

template <typename BufferStruct>
class WGPUBackendVertexArrayBuffer : public WGPUBackendArrayBuffer<BufferStruct> {
public:
    WGPUBackendVertexArrayBuffer() = default;

    void Init(const WGPUDevice& device, const char* bufferLabel, u32 sizeLimit) {
        WGPUBackendArrayBuffer<BufferStruct>::Init(device, WGPUBufferUsage_Vertex, bufferLabel, sizeLimit);
    }

    void BindToRenderPassAsVertexBuffer(const WGPURenderPassEncoder& encoder) {
        wgpuRenderPassEncoderSetVertexBuffer(
            encoder, 0, this->m_bufferDat, 0, this->m_currentBufferSize * this->m_bufferSlotStride);
    }
};

template <typename BufferStruct>
class WGPUBackendIndexArrayBuffer : public WGPUBackendArrayBuffer<BufferStruct> {
public:
    WGPUBackendIndexArrayBuffer() = default;

    void Init(const WGPUDevice& device, const char* bufferLabel, u32 sizeLimit) {
        WGPUBackendArrayBuffer<BufferStruct>::Init(device, WGPUBufferUsage_Index, bufferLabel, sizeLimit);
    }

    void BindToRenderPassAsIndexBuffer(const WGPURenderPassEncoder& encoder) {
        wgpuRenderPassEncoderSetIndexBuffer(
            encoder, this->m_bufferDat, WGPUIndexFormat_Uint32, 0, this->m_currentBufferSize * this->m_bufferSlotStride);
    }
};

// Encapsulates a buffer holding a array/vector of structs to be put into a single 
// binded storage entry and dynamically changes buffer size based inputted data.
template <typename StorageStruct>
class WGPUBackendSingleStorageArrayBuffer : public WGPUBackendArrayBuffer<StorageStruct>, public virtual WGPUBackendBindGroup::DirtyMarkingBindGroupEntry {
protected:
    void UpdateBindGroups() {
        // Updates command buffer entry accordingly
        m_currentBindGroupEntry.buffer = WGPUBackendArrayBuffer<StorageStruct>::m_bufferDat;
        m_currentBindGroupEntry.size = WGPUBackendArrayBuffer<StorageStruct>::m_currentBufferAllocatedSize * this->m_bufferSlotStride;

        WGPUBackendBindGroup::DirtyMarkingBindGroupEntry::DirtyBindingGroups();
    }

protected:

    void ResizeTo(const WGPUDevice& device, const WGPUQueue& queue, u32 newStructDataSize) override {
        WGPUBackendArrayBuffer<StorageStruct>::ResizeTo(device, queue, newStructDataSize);
        m_currentBindGroupEntry.buffer = WGPUBackendArrayBuffer<StorageStruct>::m_bufferDat;
        m_currentBindGroupEntry.size = WGPUBackendArrayBuffer<StorageStruct>::m_currentBufferAllocatedSize * this->m_bufferSlotStride;
        UpdateBindGroups();
    }

    void ResizeToTransient(const WGPUDevice& device, u32 newStructDataSize) override {
        WGPUBackendArrayBuffer<StorageStruct>::ResizeToTransient(device, newStructDataSize);
        UpdateBindGroups();
    }

public:
    WGPUBackendSingleStorageArrayBuffer() = default;
    ~WGPUBackendSingleStorageArrayBuffer() = default;
    // For now begin size and size limits describe amount of given struct that can be, not the byte size
    void Init(const WGPUDevice& device, const char* bufferLabel, u32 sizeLimit) {
        WGPUBackendArrayBuffer<StorageStruct>::Init(device, WGPUBufferUsage_Storage, bufferLabel, sizeLimit);
        m_currentBindGroupEntry = WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .buffer = WGPUBackendArrayBuffer<StorageStruct>::m_bufferDat,
            .offset = 0,
            .size = sizeof(StorageStruct)
        };
    }
};

// Represents a buffer holding a single struct
// to be put into a single binded uniform entry
// Does not ever update bind groups
template <typename UniformStruct>
class WGPUBackendSingleUniformBuffer : public WGPUBackendBindGroup::WGPUBackendBindGroupEntry {
private:
    // The encapsulated uniform buffer (Will not really ever change)
    WGPUBuffer m_bufferDat;
    
public:
    // Uninitialized uniform buffer
    WGPUBackendSingleUniformBuffer() = default;

    void Init(const WGPUDevice& device, const char* bufferLabel) {
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

/**
 * Represents a buffer holding multiple of a struct switched through dynamic offsets
 * When added in as regular entry it acts as a padded storage array 
 * When added in as dynamic uniform entry it acts as a uniform entry that can be turned into 
 */
template <typename UniformStruct>
class WGPUBackendDynamicUniformBuffer : public WGPUBackendSingleStorageArrayBuffer<UniformStruct>, public WGPUBackendDynamicUniformStore {

protected:
    // Resizing destroys and recreates the underlying buffer. The base class refreshes
    // m_currentBindGroupEntry (the storage-style descriptor), but the dynamic uniform entry
    // caches its own buffer handle and must be re-pointed too, otherwise a rebuilt dynamic
    // bind group references the destroyed buffer.
    void ResizeTo(const WGPUDevice& device, const WGPUQueue& queue, u32 newStructDataSize) override {
        WGPUBackendSingleStorageArrayBuffer<UniformStruct>::ResizeTo(device, queue, newStructDataSize);
        m_currentDynamicUniformEntry.buffer = WGPUBackendArrayBuffer<UniformStruct>::m_bufferDat;
    }

    void ResizeToTransient(const WGPUDevice& device, u32 newStructDataSize) override {
        WGPUBackendSingleStorageArrayBuffer<UniformStruct>::ResizeToTransient(device, newStructDataSize);
        m_currentDynamicUniformEntry.buffer = WGPUBackendArrayBuffer<UniformStruct>::m_bufferDat;
    }

    public:
    // Uninitialized uniform buffer
    WGPUBackendDynamicUniformBuffer() = default;

    void Init(const WGPUDevice& device, u32 dynamicStride,const char* bufferLabel, u32 sizeLimit) {
        ASSERT_PRINT(dynamicStride >= sizeof(UniformStruct), "Given uniform struct cannot fit in designated dynamic stride");
        WGPUBackendArrayBuffer<UniformStruct>::InitToSize(
            device, 
            WGPUBufferUsage_Uniform | WGPUBufferUsage_Storage, 
            bufferLabel, 
            sizeLimit, 
            dynamicStride);
        m_currentBindGroupEntry = WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .buffer = WGPUBackendArrayBuffer<UniformStruct>::m_bufferDat,
            .offset = 0,
            .size = dynamicStride
        };
        m_currentDynamicUniformEntry = WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .buffer = WGPUBackendArrayBuffer<UniformStruct>::m_bufferDat,
            .offset = 0,
            .size = sizeof(UniformStruct)
        };
        m_dynamicStride = dynamicStride;
        this->m_bufferSlotStride = dynamicStride;
    }

    // For now we don't need to use this and inherited logic assumes contiguous memory
    void EraseRange(WGPUDevice& device, WGPUQueue& queue, u32 beginIdx, u32 count) = delete;

    using WGPUBackendArrayBuffer<UniformStruct>::AppendToBack;
    virtual void AppendToBack(const WGPUDevice& device, const WGPUQueue& queue, const UniformStruct* data, u32 structLength) override {
        // Checks if allocated data has enough space to fit in data.
        // Accordingly adjusts size.
        if(structLength + this->m_currentBufferSize > this->m_currentBufferAllocatedSize) {
            this->ResizeTo(device, queue, this->CalculateNewStructSize(structLength + this->m_currentBufferSize));
        }

        // Actually writes to queue, skips writing empty space between strides
        for (u32 structIter = 0 ; structIter < structLength ; structIter++) {
            wgpuQueueWriteBuffer(
                queue, 
                this->m_bufferDat, 
                (this->m_currentBufferSize + structIter) * m_dynamicStride, 
                &data[structIter], 
                sizeof(UniformStruct));
        }
        this->m_currentBufferSize += structLength;
    }

    using WGPUBackendArrayBuffer<UniformStruct>::WriteBuffer;
    virtual void WriteBuffer(const WGPUDevice& device, const WGPUQueue& queue, const UniformStruct* data, u32 structLength) override {
        if (structLength == 0) {
            return;
        }
        // Resizes all relevant bind groups to fit in
        if(structLength > this->m_currentBufferAllocatedSize) {
            this->ResizeToTransient(device, this->CalculateNewStructSize(structLength));
        }
        
        // Actually writes to queue, skips writing empty space between strides
        for (u32 structIter = 0 ; structIter < structLength ; structIter++) {
            wgpuQueueWriteBuffer(queue, this->m_bufferDat, structIter * m_dynamicStride, &data[structIter], sizeof(UniformStruct));
        }
        this->m_currentBufferSize = structLength;
    }
};

class WGPUBackendSampler : public WGPUBackendBindGroup::DirtyMarkingBindGroupEntry {
private:
    // Encapsulated sampler data
    WGPUSampler m_samplerData{ nullptr };
    bool m_inited{ false };

public:
    // Uninitialized uniform buffer
    WGPUBackendSampler() = default;

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
        const std::string& label
        ) {

        if (m_samplerData != nullptr) {
            wgpuSamplerRelease(m_samplerData);
        }

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
            .maxAnisotropy = maxAnisotropy
        };
    
        m_samplerData = wgpuDeviceCreateSampler(device, &samplerDesc);

        m_currentBindGroupEntry = {
            .sampler = m_samplerData
        };

        m_inited = true;
        WGPUBackendBindGroup::DirtyMarkingBindGroupEntry::DirtyBindingGroups();
    }

    // Ensures no copy is made to avoid wgpu object reference conflicts
    WGPUBackendSampler(const WGPUBackendSampler&) = delete;
    WGPUBackendSampler& operator=(const WGPUBackendSampler&) = delete;
    WGPUBackendSampler(WGPUBackendSampler&&) = delete;
    WGPUBackendSampler& operator=(WGPUBackendSampler&&) = delete;
};
 
