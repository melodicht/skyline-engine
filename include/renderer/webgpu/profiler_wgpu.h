#pragma once 

#include <set>
#include <thread>
#include <atomic>

#include <webgpu.h>

#include "render_backend.h"
#include "skl_math_types.h"




class WebGpuProfiler {
private:
    // #if SKL_ENABLED_PROFILING
    std::vector<std::string> m_zones;
    std::vector<bool> m_zoneBitMask;
    WGPUBuffer m_queryResolveBuffer{ };
    WGPUBuffer m_cpuMappingBuffer{ };
    WGPUQuerySet m_querySet{ };
    const uint32_t m_maxFrames{ 0 };
    const uint32_t m_maxZoneTypes{ 0 };

    std::vector<uint32_t> m_zoneIds{ };
    std::vector<uint64_t> m_frameEndIndices{ };
    
    uint64_t m_currentFramesUsed{ 0 };
    uint64_t m_totalFrameCounter{ 0 }; 

    // Holds data needed to for AsyncFillFrameBuffer
    struct FillFrameBufferSyncInfo {
        std::mutex m_fillBufferMut{ };
        std::atomic<bool> m_filledFrameBufferInTransit{ false };
        bool m_profilerDestructed{ false };
    };
    std::shared_ptr<FillFrameBufferSyncInfo> m_syncData = std::make_shared<FillFrameBufferSyncInfo>();
    std::vector<RenderFramePerformanceInfo> m_frameBuffer{ };

    struct FillFrameBufferInfo {
        // Copied buffers needed 
        std::vector<uint32_t> m_zoneIdsCpy;
        std::vector<uint64_t> m_frameEndIndicesCpy;
        std::vector<std::string> m_zones;

        // Syncing mechanisms
        std::shared_ptr<FillFrameBufferSyncInfo> m_syncData;
        // Synced data between profiler and callback
        WGPUBuffer* m_mappedBuffer;
        std::vector<RenderFramePerformanceInfo>* m_frameBuffer;
    };
    uint32_t RegisterZone(const std::string zone);
    void AsyncFillFrameBuffer(WGPUDevice device, WGPUQueue queue);
    static void FillFrameBuffer(
        WGPUMapAsyncStatus status,
        WGPUStringView message,
        void* info,
        void* _);
    // #endif

    
public:
    // Affixes a set number of zones to be run each frame
    WebGpuProfiler(WGPUDevice device, uint32_t maxZoneTypes, uint32_t frameBufferSize);
    ~WebGpuProfiler();

    // Allows for the starting and ending of zones on
    void StartZone(WGPUCommandEncoder encoder, const std::string zoneName);
    void EndZone(WGPUCommandEncoder encoder, const std::string zoneName);

    // Marks that no zones should still be active and that a frame has been finished 
    void MarkFrameEnd(WGPUDevice device, WGPUQueue queue);

    std::vector<RenderFramePerformanceInfo> FlushRecordedTimes();
};