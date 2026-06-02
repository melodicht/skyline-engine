#pragma once

#include <webgpu/webgpu.h>
#include <guard.h>


class WGPUBackendUtils {
public:
    static WGPUStringView wgpuStr(const char* string);
};

typedef Guard<WGPUPipelineLayout, wgpuPipelineLayoutRelease> WGPUGuardedPipelineLayout;

typedef Guard<WGPUShaderModule, wgpuShaderModuleRelease> WGPUGuardedShaderModule;

typedef Guard<WGPUBindGroupLayout, wgpuBindGroupLayoutRelease> WGPUGuardedBindGroupLayout;