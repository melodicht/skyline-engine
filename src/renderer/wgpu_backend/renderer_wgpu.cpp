#include "renderer/wgpu_backend/renderer_wgpu.h"

#include "renderer/wgpu_backend/utils_wgpu.h"

#include "webgpu/sdl3webgpu-main/sdl3webgpu.h"

#include "skl_logger.h"

#include "math/skl_math_utils.h"

#include "renderer/wgpu_backend/dynamic_light_converter.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif

#include <cstdlib>
#include <cstring>
#include <iostream>

#if SKL_ENABLED_EDITOR
#include <backends/imgui_impl_wgpu.h>
#endif

// Much of this was taken from https://eliemichel.github.io/LearnWebGPU

// TODO: Make such constants more configurable
constexpr u32 DefaultCascadeCount = 4;

#pragma region Helper Functions
void WGPURenderBackend::printDeviceSpecs() {
  WGPUSupportedFeatures features;
  wgpuDeviceGetFeatures(m_wgpuCore.m_device, &features);

  LOG("Device features:");
  for (int iter = 0; iter < features.featureCount ; iter++) {
      LOG(" - 0x" << features.features[iter]);
  }

  WGPULimits limits = {};
  limits.nextInChain = nullptr;

  WGPUStatus success = wgpuDeviceGetLimits(m_wgpuCore.m_device, &limits);

  if (success == WGPUStatus_Success) {
      LOG("Device limits:");
      LOG(" - maxBindGroups: " << limits.maxBindGroups);
      LOG(" - maxBindGroupsPlusVertexBuffers: " << limits.maxBindGroupsPlusVertexBuffers);
      LOG(" - maxBindingsPerBindGroup: " << limits.maxBindingsPerBindGroup);
      LOG(" - maxBufferSize: " << limits.maxBufferSize);
      LOG(" - maxColorAttachmentBytesPerSample: " << limits.maxColorAttachmentBytesPerSample);
      LOG(" - maxColorAttachments: " << limits.maxColorAttachments);
      LOG(" - maxComputeInvocationsPerWorkgroup: " << limits.maxComputeInvocationsPerWorkgroup);
      LOG(" - maxComputeWorkgroupSizeX: " << limits.maxComputeWorkgroupSizeX);
      LOG(" - maxComputeWorkgroupSizeY: " << limits.maxComputeWorkgroupSizeY);
      LOG(" - maxComputeWorkgroupSizeZ: " << limits.maxComputeWorkgroupSizeZ);
      LOG(" - maxTextureDimension1D: " << limits.maxTextureDimension1D);
      LOG(" - maxTextureDimension2D: " << limits.maxTextureDimension2D);
      LOG(" - maxTextureDimension3D: " << limits.maxTextureDimension3D);
      LOG(" - maxTextureArrayLayers: " << limits.maxTextureArrayLayers);
      // [...] Extra device limits
  }
}

WGPUBindGroupLayoutEntry DefaultBindLayoutEntry() {
  return WGPUBindGroupLayoutEntry {
    .nextInChain = nullptr,
    .binding = 0,
    .visibility = WGPUShaderStage_None,
    .buffer {
      .nextInChain = nullptr,
      .type = WGPUBufferBindingType_BindingNotUsed,
      .hasDynamicOffset = false,
      .minBindingSize = 0,
    },
    .sampler {
      .nextInChain = nullptr,
      .type = WGPUSamplerBindingType_BindingNotUsed,
    },
    .texture {
      .nextInChain = nullptr,
      .sampleType = WGPUTextureSampleType_BindingNotUsed,
      .viewDimension =  WGPUTextureViewDimension_Undefined,
      .multisampled = false,
    },
    .storageTexture {
      .nextInChain = nullptr,
      .access = WGPUStorageTextureAccess_BindingNotUsed,
      .format = WGPUTextureFormat_Undefined,
      .viewDimension = WGPUTextureViewDimension_Undefined,
    },
  };
}

WGPUAdapter WGPURenderBackend::GetAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
  WGPUAdapter set = nullptr;
  bool requestEnded = false;

  WGPURequestAdapterCallbackInfo callbackInfo;
  callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapterImpl* adapter, WGPUStringView message, void *userdata1, void *userdata2) 
  {
      if (status == WGPURequestAdapterStatus_Success) {
        *((WGPUAdapter *)userdata1) = adapter;
      } else {
          // LOG("Could not get WebGPU adapter: " << (message.data));
          LOG("Could not get WebGPU adapter: " << (message.data));
      }
      *(bool *)userdata2 = true;
  };

  callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  callbackInfo.userdata1 = &set;
  callbackInfo.userdata2 = &requestEnded;

  // Call to the WebGPU request adapter procedure
  wgpuInstanceRequestAdapter(
      instance /* equivalent of navigator.gpu */,
      options,
      callbackInfo
  );
  // We wait until userData.requestEnded gets true
  #if EMSCRIPTEN
  while (!requestEnded) {
      emscripten_sleep(100);
  }
  #endif

  assert(requestEnded);

  return set;
}

WGPUDevice WGPURenderBackend::GetDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
  WGPUDevice set = nullptr;
  bool requestEnded = false;

  WGPURequestDeviceCallbackInfo callbackInfo;
  callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void * userdata1, void * userdata2) {
      if (status == WGPURequestDeviceStatus_Success) {
        *((WGPUDevice *)userdata1) = device;
      } else {
          LOG("Could not get WebGPU device: " << message.data);
      }
      *((bool *)userdata2) = true;
  };
  callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  callbackInfo.userdata1 = &set;
  callbackInfo.userdata2 = &requestEnded;

  wgpuAdapterRequestDevice(
      adapter,
      descriptor,
      callbackInfo
  );

#ifdef __EMSCRIPTEN__
  while (!requestEnded) {
      emscripten_sleep(100);
  }
#endif // __EMSCRIPTEN__

  assert(requestEnded);

  return set;
}

void WGPURenderBackend::QueueFinishCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
  LOG("Queued work finished with status: " << status);
  LOG("Included Message: " << message.data);
}

void WGPURenderBackend::LostDeviceCallback(WGPUDevice const * device, WGPUDeviceLostReason reason, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
  LOG("Device lost: reason " << reason);
  if (message.data) LOG(" (" << message.data << ")");
}

void WGPURenderBackend::ErrorCallback(WGPUDevice const * device, WGPUErrorType type, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
  LOG("Error happened: error " << type);
  if (message.data) LOG("(" << message.data << ")");
}

bool WGPURenderBackend::InitFrame() {
  #if SKL_ENABLED_EDITOR
  ImGui_ImplWGPU_NewFrame();
  #endif

  // Gets current color texture
  wgpuSurfaceGetCurrentTexture(m_wgpuSurface, &m_surfaceTexture);

  if (m_surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
      return false;
  }

  WGPUTextureViewDescriptor viewDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Initializing texture view"),
    .format = wgpuTextureGetFormat(m_surfaceTexture.texture),
    .dimension = WGPUTextureViewDimension_2D,
    .baseMipLevel = 0,
    .mipLevelCount = 1,
    .baseArrayLayer = 0,
    .arrayLayerCount = 1,
    .aspect = WGPUTextureAspect_All,
    .usage = WGPUTextureUsage_RenderAttachment
  };

  m_surfaceTextureView = wgpuTextureCreateView(m_surfaceTexture.texture, &viewDescriptor);

  if(!m_surfaceTextureView)
  {
    return false;
  }

  return true;
}

void WGPURenderBackend::EndFrame() {
  if (m_surfaceTextureView) {
    wgpuTextureViewRelease(m_surfaceTextureView);
  }

  #ifndef __EMSCRIPTEN__
  wgpuSurfacePresent(m_wgpuSurface);
  wgpuInstanceProcessEvents(m_wgpuCore.m_instance);  
  #else
    
  emscripten_sleep(10);
  #endif
}

void WGPURenderBackend::DrawObjects(std::map<MeshID, u32>& meshCounts) {
  u32 startIndex = 0;
  for (std::pair<MeshID, u32> pair : meshCounts)
  {
    WGPUBackendMeshIdx& gotMesh = m_meshStore[pair.first];
    wgpuRenderPassEncoderDrawIndexed(m_renderPassEncoder, gotMesh.m_indexCount, pair.second, gotMesh.m_baseIndex, gotMesh.m_baseVertex, startIndex);
    startIndex += pair.second;
  }
}

void WGPURenderBackend::BeginColorPass() {
  assert(!m_renderPassActive);

  m_renderPassActive = true;

  // Create a command encoder for the draw call
  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Encoder Descriptor")
  };
  m_passCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);

  WGPURenderPassColorAttachment startPass {
    .view = m_surfaceTextureView,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .resolveTarget = nullptr,
    .loadOp = WGPULoadOp_Clear,
    .storeOp = WGPUStoreOp_Store,
    .clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 }
  };

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = m_depthTexture.m_textureView,
    .depthClearValue = 1.0f,
    .depthReadOnly = true,
    .stencilReadOnly = true,
  };

  WGPURenderPassDescriptor colorPassDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Descriptor"),
    .colorAttachmentCount = 1,
    .colorAttachments = &startPass,
    .depthStencilAttachment = &depthStencilAttachment,
    .timestampWrites = nullptr,
  };
    
  m_renderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_passCommandEncoder, &colorPassDescriptor);

  wgpuRenderPassEncoderSetPipeline(m_renderPassEncoder, m_defaultPipeline);

  m_bindGroup.BindToRenderPass(m_renderPassEncoder);

  m_meshVertexBuffer.BindToRenderPassAsVertexBuffer(m_renderPassEncoder);
  m_meshIndexBuffer.BindToRenderPassAsIndexBuffer(m_renderPassEncoder); 
}

void WGPURenderBackend::BeginDepthPass(WGPUTextureView depthTexture) {
  assert(!m_renderPassActive);

  m_renderPassActive = true;

  // Create a command encoder for the draw call
  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Encoder Descriptor")
  };
  m_passCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  WGPURenderPassDescriptor depthPassDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pass Descriptor"),
    .colorAttachmentCount = 0,
    .colorAttachments = nullptr,
    .depthStencilAttachment = &depthStencilAttachment,
    .timestampWrites = nullptr,
  };

  m_renderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_passCommandEncoder, &depthPassDescriptor);

  wgpuRenderPassEncoderSetPipeline(m_renderPassEncoder, m_depthPipeline);

  m_depthBindGroup.BindToRenderPass(m_renderPassEncoder);
  m_meshVertexBuffer.BindToRenderPassAsVertexBuffer(m_renderPassEncoder);
  m_meshIndexBuffer.BindToRenderPassAsIndexBuffer(m_renderPassEncoder);
}

void WGPURenderBackend::BeginPointDepthPass(WGPUTextureView depthTexture) {
  assert(!m_renderPassActive);

  m_renderPassActive = true;

  // Create a command encoder for the draw call
  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Encoder Descriptor")
  };
  m_passCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  WGPURenderPassDescriptor depthPassDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pass Descriptor"),
    .colorAttachmentCount = 0,
    .colorAttachments = nullptr,
    .depthStencilAttachment = &depthStencilAttachment,
    .timestampWrites = nullptr,
  };

  m_renderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_passCommandEncoder, &depthPassDescriptor);

  wgpuRenderPassEncoderSetPipeline(m_renderPassEncoder, m_pointDepthPipeline);

  m_pointDepthBindGroup.BindToRenderPass(m_renderPassEncoder);
  m_meshVertexBuffer.BindToRenderPassAsVertexBuffer(m_renderPassEncoder);
  m_meshIndexBuffer.BindToRenderPassAsIndexBuffer(m_renderPassEncoder);
}

void WGPURenderBackend::EndPass() {
  m_renderPassActive = false;

  wgpuRenderPassEncoderEnd(m_renderPassEncoder);
  wgpuRenderPassEncoderRelease(m_renderPassEncoder);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {
    .nextInChain = nullptr,
    .label =  WGPUBackendUtils::wgpuStr("Ending pass command buffer"),
  };

  WGPUCommandBuffer passCommand = wgpuCommandEncoderFinish(m_passCommandEncoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(m_passCommandEncoder);

  wgpuQueueSubmit(m_wgpuQueue, 1, &passCommand);
  wgpuCommandBufferRelease(passCommand);
}

void WGPURenderBackend::DrawImGui() {
  #if SKL_ENABLED_EDITOR

  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Imgui Encoder Descriptor")
  };
  WGPUCommandEncoder imguiCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);

  WGPURenderPassColorAttachment meshColorPass {
    .view = m_surfaceTextureView,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .resolveTarget = nullptr,
    .loadOp = WGPULoadOp_Load,
    .storeOp = WGPUStoreOp_Store,
  };

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = m_depthTexture.m_textureView,
    .depthLoadOp = WGPULoadOp_Load,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  WGPURenderPassDescriptor meshPassDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Imgui render pass"),
    .colorAttachmentCount = 1,
    .colorAttachments = &meshColorPass,
    .depthStencilAttachment = &depthStencilAttachment,
    .timestampWrites = nullptr,
  };

  WGPURenderPassEncoder imguiPassEncoder = wgpuCommandEncoderBeginRenderPass(imguiCommandEncoder, &meshPassDesc);

  ImGui::Render();
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), imguiPassEncoder);

  wgpuRenderPassEncoderEnd(imguiPassEncoder);
  wgpuRenderPassEncoderRelease(imguiPassEncoder);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {
    .nextInChain = nullptr,
    .label =  WGPUBackendUtils::wgpuStr("Imgui Command Buffer"),
  };

  WGPUCommandBuffer imguiCommand = wgpuCommandEncoderFinish(imguiCommandEncoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(imguiCommandEncoder);

  wgpuQueueSubmit(m_wgpuQueue, 1, &imguiCommand);
  wgpuCommandBufferRelease(imguiCommand);
  #endif
}
#pragma endregion

#pragma region Interface Impl
WGPURenderBackend::~WGPURenderBackend() {
  wgpuSurfaceUnconfigure(m_wgpuSurface);
  wgpuSurfaceRelease(m_wgpuSurface);
  wgpuQueueRelease(m_wgpuQueue);
}

void WGPURenderBackend::InitRenderer(SDL_Window *window, u32 startWidth, u32 startHeight) {
  m_screenWidth = startWidth;
  m_screenHeight = startHeight;
  // Creates instance
  WGPUInstanceDescriptor instanceDescriptor { 
    .nextInChain = nullptr
  };

  #if EMSCRIPTEN
  m_wgpuCore.m_instance = wgpuCreateInstance(nullptr);
  #else
  m_wgpuCore.m_instance = wgpuCreateInstance(&instanceDescriptor);
  #endif
  if (m_wgpuCore.m_instance == nullptr) {
    LOG_ERROR("Instance creation failed!");
    return;
  }
  LOG("Instance Created" << m_wgpuCore.m_instance);

  LOG("Requesting adapter...");

  m_wgpuSurface = SDL_GetWGPUSurface(m_wgpuCore.m_instance, window);
  WGPURequestAdapterOptions adapterOpts = {
    .nextInChain = nullptr,
    .compatibleSurface = m_wgpuSurface
  };

  WGPUAdapter adapter = GetAdapter(m_wgpuCore.m_instance, &adapterOpts);

  LOG("Got adapter: " << adapter);

  LOG("Requesting device...");

  // General device description
  WGPULimits deviceRequirements = WGPU_LIMITS_INIT;
  deviceRequirements.maxTextureArrayLayers = 2048;

  WGPUDeviceDescriptor deviceDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("My Device"),
    .requiredFeatureCount = 0,
    .requiredLimits = &deviceRequirements,
    .defaultQueue {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Default Queue")
    },
    .deviceLostCallbackInfo {
      .mode = WGPUCallbackMode_AllowSpontaneous,
      .callback = LostDeviceCallback
    },
    .uncapturedErrorCallbackInfo {
      .nextInChain = nullptr,
      .callback = ErrorCallback
    }
  };

  m_wgpuCore.m_device = GetDevice(adapter, &deviceDesc);

  LOG("Got device: " << m_wgpuCore.m_device);

  printDeviceSpecs();

  m_wgpuQueue = wgpuDeviceGetQueue(m_wgpuCore.m_device);

  WGPUQueueWorkDoneCallbackInfo queueDoneCallback =  WGPUQueueWorkDoneCallbackInfo {
    .mode = WGPUCallbackMode_AllowProcessEvents,
    #if EMSCRIPTEN // TODO: It seems that emdawn has split off from native for now, check frequently
    .callback = nullptr,
    #else
    .callback = QueueFinishCallback,
    #endif
  };

  wgpuQueueOnSubmittedWorkDone(m_wgpuQueue, queueDoneCallback);

  WGPUSurfaceCapabilities capabilities { };
  wgpuSurfaceGetCapabilities(m_wgpuSurface, adapter, &capabilities );
  m_wgpuTextureFormat = capabilities.formats[0];
  WGPUSurfaceConfiguration config { 
    .nextInChain = nullptr,
    .device = m_wgpuCore.m_device,
    .format = m_wgpuTextureFormat,
    .usage = WGPUTextureUsage_RenderAttachment,
    .width = startWidth,
    .height = startHeight,
    .viewFormatCount = 0,
    .viewFormats = nullptr,
    .alphaMode = WGPUCompositeAlphaMode_Auto,
    .presentMode = WGPUPresentMode_Fifo
  };

  wgpuSurfaceCapabilitiesFreeMembers( capabilities );

  wgpuSurfaceConfigure(m_wgpuSurface, &config);
  wgpuAdapterRelease(adapter);

  // Creates vertex/index buffers
  m_meshVertexBuffer.Init(m_wgpuCore.m_device, WGPUBufferUsage_Vertex, "WGPUBackendMeshIdx Vertex Buffer", m_maxMeshVertSize);

  m_meshIndexBuffer.Init(m_wgpuCore.m_device, WGPUBufferUsage_Index, "WGPUBackendMeshIdx Vertex Buffer", m_maxMeshIndexSize);

  // Creates depth texture 
  WGPUTextureDescriptor depthTextureDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Surface texture view"),
    .usage = WGPUTextureUsage_RenderAttachment,
    .dimension = WGPUTextureDimension_2D,
    .size = {startWidth, startHeight, 1},
    .format = m_wgpuDepthTextureFormat,
    .mipLevelCount = 1,
    .sampleCount = 1,
    .viewFormatCount = 1,
    .viewFormats = &m_wgpuDepthTextureFormat,
  };
  m_depthTexture.m_texture = wgpuDeviceCreateTexture(m_wgpuCore.m_device, &depthTextureDescriptor);

  WGPUTextureViewDescriptor depthViewDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Start depth view descriptor"),
    .format = m_wgpuDepthTextureFormat,
    .dimension = WGPUTextureViewDimension_2D,
    .baseMipLevel = 0,
    .mipLevelCount = 1,
    .baseArrayLayer = 0,
    .arrayLayerCount = 1,
    .aspect = WGPUTextureAspect_DepthOnly,
  };

  m_depthTexture.m_textureView = wgpuTextureCreateView(m_depthTexture.m_texture, &depthViewDescriptor);

  // Initializes imgui
  #if SKL_ENABLED_EDITOR
  ImGui_ImplWGPU_InitInfo imguiInit;
  imguiInit.Device = m_wgpuCore.m_device;
  imguiInit.RenderTargetFormat = m_wgpuTextureFormat;
  imguiInit.DepthStencilFormat = m_wgpuDepthTextureFormat;
  imguiInit.NumFramesInFlight = 3;

  ImGui_ImplWGPU_Init(&imguiInit);

  ImGui_ImplWGPU_NewFrame();
  #endif
}

WGPUShaderModule loadShader(const WGPUDevice& device, std::string fileName, std::string shaderLabel) {
  // Loads in shader module
  size_t loadedDatSize;
  auto loadedDat = SDL_LoadFile(fileName.data(), &loadedDatSize);

  // Makes sure data actually gets loaded in
  assert(loadedDat);

  WGPUShaderModuleWGSLDescriptor wgslShaderDesc {
    .chain {
      .next = nullptr,
      .sType = WGPUSType_ShaderSourceWGSL,
    },
    .code{
      .data = reinterpret_cast<const char *>(loadedDat),
      .length = loadedDatSize,
    },
  };

  WGPUShaderModuleDescriptor shaderDesc {
    .nextInChain = &wgslShaderDesc.chain,
    .label = WGPUBackendUtils::wgpuStr(shaderLabel.data()),
  };

  WGPUShaderModule retModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
  SDL_free(loadedDat);
  return retModule;
}

void WGPURenderBackend::InitPipelines()
{
  WGPUShaderModule shaderModule = loadShader(m_wgpuCore.m_device, "shaders/default_shader.wgsl", "Color Pass Shader");

  WGPUShaderModule depthShaderModule = loadShader(m_wgpuCore.m_device, "shaders/depth_shader.wgsl", "Depth Pass Shader");

  WGPUShaderModule pointDepthShaderModule = loadShader(m_wgpuCore.m_device, "shaders/point_depth_shader.wgsl", "Point Depth Pass Shader");
  // Configures z-buffer
  WGPUDepthStencilState depthStencilReadOnlyState {
    .nextInChain = nullptr,
    .format = m_wgpuDepthTextureFormat,
    .depthWriteEnabled = WGPUOptionalBool_False,
    .depthCompare = WGPUCompareFunction_LessEqual,
    .stencilFront {
      .compare = WGPUCompareFunction_Always,
      .failOp = WGPUStencilOperation_Keep,
      .depthFailOp = WGPUStencilOperation_Keep,
      .passOp = WGPUStencilOperation_Keep
    },
    .stencilBack {
      .compare = WGPUCompareFunction_Always,
      .failOp = WGPUStencilOperation_Keep,
      .depthFailOp = WGPUStencilOperation_Keep,
      .passOp = WGPUStencilOperation_Keep
    },
    .stencilReadMask = 0,
    .stencilWriteMask = 0,
    .depthBias = 0,
    .depthBiasSlopeScale = 0,
    .depthBiasClamp = 0,
  };

  WGPUDepthStencilState depthStencilState {
    .nextInChain = nullptr,
    .format = m_wgpuDepthTextureFormat,
    .depthWriteEnabled = WGPUOptionalBool_True,
    .depthCompare = WGPUCompareFunction_LessEqual,
    .stencilFront {
      .compare = WGPUCompareFunction_Always,
      .failOp = WGPUStencilOperation_Keep,
      .depthFailOp = WGPUStencilOperation_Keep,
      .passOp = WGPUStencilOperation_Keep
    },
    .stencilBack {
      .compare = WGPUCompareFunction_Always,
      .failOp = WGPUStencilOperation_Keep,
      .depthFailOp = WGPUStencilOperation_Keep,
      .passOp = WGPUStencilOperation_Keep
    },
    .stencilReadMask = 0,
    .stencilWriteMask = 0,
    .depthBias = 0,
    .depthBiasSlopeScale = 0,
    .depthBiasClamp = 0,
  };

  WGPUBlendState blendState {
    .color {
      .operation = WGPUBlendOperation_Add,
      .srcFactor = WGPUBlendFactor_SrcAlpha,
      .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
    },
    .alpha {
      .operation = WGPUBlendOperation_Add,
      .srcFactor = WGPUBlendFactor_Zero,
      .dstFactor = WGPUBlendFactor_One,
    },
  };

  WGPUColorTargetState colorTarget {
    .nextInChain = nullptr,
    .format = m_wgpuTextureFormat,
    .blend = &blendState,
    .writeMask = WGPUColorWriteMask_All,
  };

  WGPUFragmentState fragState {
    .module = shaderModule,
    .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
    .constantCount = 0,
    .constants = nullptr,
    .targetCount = 1,
    .targets = &colorTarget,
  };

  WGPUFragmentState pointDepthDummyFragState {
    .module = pointDepthShaderModule,
    .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
    .constantCount = 0,
    .constants = nullptr,
    .targetCount = 0,
    .targets = nullptr,
  };

  std::vector<WGPUVertexAttribute> vertexAttributes{ };
  std::vector<WGPUVertexAttribute> depthVertexAttributes{ };
  std::vector<WGPUVertexAttribute> pointDepthVertexAttributes{ };

  WGPUVertexAttribute posVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32x3,
    .offset = 0,
    .shaderLocation = 0,
  };
  vertexAttributes.push_back(posVertAttribute);
  depthVertexAttributes.push_back(posVertAttribute);
  pointDepthVertexAttributes.push_back(posVertAttribute);

  WGPUVertexAttribute uvXVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32,
    .offset = sizeof(glm::vec3),
    .shaderLocation = 1,
  };
  vertexAttributes.push_back(uvXVertAttribute);

  WGPUVertexAttribute normVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32x3,
    .offset =  sizeof(glm::vec3) + sizeof(float),
    .shaderLocation = 2,
  };
  vertexAttributes.push_back(normVertAttribute);
  depthVertexAttributes.push_back(normVertAttribute);
  pointDepthVertexAttributes.push_back(normVertAttribute);

  WGPUVertexAttribute uvYVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32,
    .offset =  sizeof(glm::vec3) * 2 + sizeof(float),
    .shaderLocation = 3,
  };
  vertexAttributes.push_back(uvYVertAttribute);


  WGPUVertexBufferLayout bufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(float) * 2,
    .attributeCount = vertexAttributes.size(),
    .attributes = vertexAttributes.data(),
  };

  WGPUVertexBufferLayout depthBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(float) * 2,
    .attributeCount = depthVertexAttributes.size(),
    .attributes = depthVertexAttributes.data(),
  };

  WGPUVertexBufferLayout pointDepthBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(float) * 2,
    .attributeCount = pointDepthVertexAttributes.size(),
    .attributes = pointDepthVertexAttributes.data(),
  };

  std::vector<WGPUBindGroupLayoutEntry> bindEntities;
  std::vector<WGPUBindGroupLayoutEntry> depthBindEntities;
  std::vector<WGPUBindGroupLayoutEntry> pointDepthBindEntities;
  
  WGPUBindGroupLayoutEntry fixedColorPassBind = DefaultBindLayoutEntry();
  fixedColorPassBind.binding = 0;
  fixedColorPassBind.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
  fixedColorPassBind.buffer.type = WGPUBufferBindingType_Uniform;
  fixedColorPassBind.buffer.minBindingSize = sizeof(WGPUBackendColorPassFixedData);
  bindEntities.push_back( fixedColorPassBind );

  WGPUBindGroupLayoutEntry cameraSpaceBind = DefaultBindLayoutEntry();
  cameraSpaceBind.binding = 0;
  cameraSpaceBind.visibility = WGPUShaderStage_Vertex;
  cameraSpaceBind.buffer.type = WGPUBufferBindingType_Uniform;
  cameraSpaceBind.buffer.minBindingSize = sizeof(glm::mat4x4);
  depthBindEntities.push_back( cameraSpaceBind );
  pointDepthBindEntities.push_back( cameraSpaceBind );

  WGPUBindGroupLayoutEntry objDatBind = DefaultBindLayoutEntry();
  objDatBind.binding = 1;
  objDatBind.visibility = WGPUShaderStage_Vertex;
  objDatBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  objDatBind.buffer.minBindingSize = sizeof(WGPUBackendObjectData);
  bindEntities.push_back( objDatBind );
  depthBindEntities.push_back( objDatBind );
  pointDepthBindEntities.push_back( objDatBind );

  WGPUBindGroupLayoutEntry fixedPointDepthPassBind = DefaultBindLayoutEntry();
  fixedPointDepthPassBind.binding = 2;
  fixedPointDepthPassBind.visibility = WGPUShaderStage_Fragment;
  fixedPointDepthPassBind.buffer.type = WGPUBufferBindingType_Uniform;
  fixedPointDepthPassBind.buffer.minBindingSize = sizeof(WGPUBackendPointDepthPassFixedData);
  pointDepthBindEntities.push_back( fixedPointDepthPassBind );

  WGPUBindGroupLayoutEntry dynamicShadowedDirLightBind = DefaultBindLayoutEntry();
  dynamicShadowedDirLightBind.binding = 2;
  dynamicShadowedDirLightBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowedDirLightBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowedDirLightBind.buffer.minBindingSize = sizeof(WGPUBackendDynamicShadowedDirLightData);
  bindEntities.push_back( dynamicShadowedDirLightBind );

  WGPUBindGroupLayoutEntry dynamicShadowedPointLightBind = DefaultBindLayoutEntry();
  dynamicShadowedPointLightBind.binding = 3;
  dynamicShadowedPointLightBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowedPointLightBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowedPointLightBind.buffer.minBindingSize = sizeof(WGPUBackendDynamicShadowedPointLightData);
  bindEntities.push_back( dynamicShadowedPointLightBind );

  WGPUBindGroupLayoutEntry dynamicShadowedSpotLightBind = DefaultBindLayoutEntry();
  dynamicShadowedSpotLightBind.binding = 4;
  dynamicShadowedSpotLightBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowedSpotLightBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowedSpotLightBind.buffer.minBindingSize = sizeof(WGPUBackendDynamicShadowedSpotLightData);
  bindEntities.push_back( dynamicShadowedSpotLightBind );

  WGPUBindGroupLayoutEntry dynamicShadowLightSpacesBind = DefaultBindLayoutEntry();
  dynamicShadowLightSpacesBind.binding = 5;
  dynamicShadowLightSpacesBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowLightSpacesBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowLightSpacesBind.buffer.minBindingSize = sizeof(glm::mat4x4);
  bindEntities.push_back( dynamicShadowLightSpacesBind );

  WGPUBindGroupLayoutEntry dynamicDirLightShadowMapBind = DefaultBindLayoutEntry();
  dynamicDirLightShadowMapBind.binding = 6;
  dynamicDirLightShadowMapBind.visibility = WGPUShaderStage_Fragment;
  dynamicDirLightShadowMapBind.texture = {
    .nextInChain = nullptr,
    .sampleType = WGPUTextureSampleType_Depth,
    .viewDimension = WGPUTextureViewDimension_2DArray,
    .multisampled = false
  };
  bindEntities.push_back( dynamicDirLightShadowMapBind );

  WGPUBindGroupLayoutEntry dynamicPointLightShadowMapBind = DefaultBindLayoutEntry();
  dynamicPointLightShadowMapBind.binding = 7;
  dynamicPointLightShadowMapBind.visibility = WGPUShaderStage_Fragment;
  dynamicPointLightShadowMapBind.texture = {
    .nextInChain = nullptr,
    .sampleType = WGPUTextureSampleType_Depth,
    .viewDimension = WGPUTextureViewDimension_CubeArray,
    .multisampled = false
  };
  bindEntities.push_back( dynamicPointLightShadowMapBind );

  WGPUBindGroupLayoutEntry dynamicDirLightCascadeRatiosBind = DefaultBindLayoutEntry();
  dynamicDirLightCascadeRatiosBind.binding = 8;
  dynamicDirLightCascadeRatiosBind.visibility = WGPUShaderStage_Fragment;
  dynamicDirLightCascadeRatiosBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicDirLightCascadeRatiosBind.buffer.minBindingSize = sizeof(float);
  bindEntities.push_back( dynamicDirLightCascadeRatiosBind );

  WGPUBindGroupLayoutEntry shadowMapSamplerMapBind = DefaultBindLayoutEntry();
  shadowMapSamplerMapBind.binding = 9;
  shadowMapSamplerMapBind.visibility = WGPUShaderStage_Fragment;
  shadowMapSamplerMapBind.sampler = {
    .nextInChain = nullptr,
    .type = WGPUSamplerBindingType_Comparison
  };
  bindEntities.push_back( shadowMapSamplerMapBind );

  WGPUBindGroupLayoutDescriptor bindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Bind Layout"),
    .entryCount = bindEntities.size(), 
    .entries = bindEntities.data(),
  };

  WGPUBindGroupLayoutDescriptor depthBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pass Bind Layout"),
    .entryCount = depthBindEntities.size(), 
    .entries = depthBindEntities.data(),
  };

  WGPUBindGroupLayoutDescriptor pointDepthBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pass Bind Layout"),
    .entryCount = pointDepthBindEntities.size(),
    .entries = pointDepthBindEntities.data(),
  };

  WGPUBindGroupLayout bindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &bindLayoutDescriptor);
  WGPUBindGroupLayout depthBindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &depthBindLayoutDescriptor);
  WGPUBindGroupLayout pointDepthBindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &pointDepthBindLayoutDescriptor);

  WGPUPipelineLayoutDescriptor pipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &bindLayout,
  };

  WGPUPipelineLayoutDescriptor depthPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &depthBindLayout,
  };

    WGPUPipelineLayoutDescriptor pointDepthPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &pointDepthBindLayout,
  };

  WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &pipelineLayoutConstructor);
  WGPUPipelineLayout depthPipelineLayout = wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &depthPipelineLayoutConstructor);
  WGPUPipelineLayout pointDepthPipelineLayout =  wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &pointDepthPipelineLayoutConstructor);

  WGPURenderPipelineDescriptor pointDepthPipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pipeline Layout"),
    .layout = pointDepthPipelineLayout,
    .vertex {
      .module = pointDepthShaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &pointDepthBufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = &pointDepthDummyFragState,
  };

  WGPURenderPipelineDescriptor depthPipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline Layout"),
    .layout = depthPipelineLayout,
    .vertex {
      .module = depthShaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &depthBufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = nullptr,
  };

  WGPURenderPipelineDescriptor pipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Default Pipeline Layout"),
    .layout = pipelineLayout,
    .vertex {
      .module = shaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &bufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilReadOnlyState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = &fragState,
  };

  m_defaultPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &pipelineDesc);
  m_depthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &depthPipelineDesc);
  m_pointDepthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &pointDepthPipelineDesc);

  m_bindGroup.Init("Color Pass Pipeline Bind Group", bindLayout);
  m_depthBindGroup.Init("Depth Pipeline Pass Bind Group", depthBindLayout);
  m_pointDepthBindGroup.Init("Point Depth Pass Bind Group", pointDepthBindLayout);

  m_cameraSpaceBuffer.Init(m_wgpuCore.m_device, "Camera Space Buffer", 0);
  m_depthBindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_cameraSpaceBuffer));
  m_pointDepthBindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_cameraSpaceBuffer));

  m_fixedColorPassDatBuffer.Init(m_wgpuCore.m_device, "Color Pass Fixed Data Buffer", 0);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_fixedColorPassDatBuffer));

  m_instanceDatBuffer.Init(m_wgpuCore.m_device, "Instance Buffer", 1, m_maxObjArraySize);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_instanceDatBuffer));
  m_depthBindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_instanceDatBuffer));
  m_pointDepthBindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_instanceDatBuffer));

  m_fixedPointDepthPassDatBuffer.Init(m_wgpuCore.m_device, "Point Depth Pass Fixed Data Buffer", 2);
  m_pointDepthBindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_fixedPointDepthPassDatBuffer));

  m_dynamicShadowedDirLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Direction Light Buffer", 2, m_maxDynamicShadowedDirLights);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicShadowedDirLightBuffer));

  m_dynamicShadowedPointLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Point Light Buffer", 3, m_maxDynamicShadowedPointLights);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicShadowedPointLightBuffer));

  m_dynamicShadowedSpotLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Dir Light Buffer", 4, m_maxDynamicShadowedSpotLights);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicShadowedSpotLightBuffer));

  m_dynamicShadowLightSpaces.Init(m_wgpuCore.m_device, "Dynamic Shadow Light Spaces", 5, m_maxDynamicShadowLightSpaces);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicShadowLightSpaces));

  // TODO: Replace placeholder 1000 x 1000 dimensions and limit
  m_dynamicDirLightShadowMapTexture.Init(
    m_wgpuCore.m_device, 
    1000, 
    1000, 
    32, 
    DefaultCascadeCount, 
    "Dynamic Direction Light Shadow Maps", 
    "Dynamic Direction Light Shadow Maps Whole", 
    "Dynamic Direction Light Shadow Maps Layer", 
    6,
    false);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicDirLightShadowMapTexture));

  m_dynamicPointLightShadowMapTexture.Init(
    m_wgpuCore.m_device, 
    1000, 
    1000, 
    2048, 
    6,
    "Dynamic Point Light Shadow Maps", 
    "Dynamic Point Light Shadow Maps Cube Arrays", 
    "Dynamic Point Light Shadow Maps Texture Layer", 
    7,
    true
  );
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicPointLightShadowMapTexture));


  m_dynamicShadowedDirLightCascadeRatiosBuffer.Init(m_wgpuCore.m_device, "Shadowed Dynamic Directional Light Cascade Ratios", 8, DefaultCascadeCount);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_dynamicShadowedDirLightCascadeRatiosBuffer));

  m_shadowMapSampler.InitOrUpdate(
    m_wgpuCore.m_device, 
    m_wgpuQueue, 
    WGPUAddressMode_ClampToEdge, 
    WGPUFilterMode_Nearest, 
    WGPUFilterMode_Nearest, 
    WGPUMipmapFilterMode_Nearest, 
    0.0, 
    0.0, 
    WGPUCompareFunction_Less, 
    1, 
    "Shadow Map Sampler", 
    9);
  m_bindGroup.AddEntryToBindingGroup(static_cast<WGPUBackendBindGroup::IWGPUBackendUniformEntry&>(m_shadowMapSampler));
  
  m_bindGroup.InitOrUpdateBindGroup(m_wgpuCore.m_device);
  m_depthBindGroup.InitOrUpdateBindGroup(m_wgpuCore.m_device);
  m_pointDepthBindGroup.InitOrUpdateBindGroup(m_wgpuCore.m_device);
    
  wgpuPipelineLayoutRelease(pointDepthPipelineLayout);
  wgpuPipelineLayoutRelease(depthPipelineLayout);
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuShaderModuleRelease(pointDepthShaderModule);
  wgpuShaderModuleRelease(depthShaderModule);
  wgpuShaderModuleRelease(shaderModule);
  wgpuBindGroupLayoutRelease(pointDepthBindLayout);
  wgpuBindGroupLayoutRelease(depthBindLayout);
  wgpuBindGroupLayoutRelease(bindLayout);
}

MeshID WGPURenderBackend::UploadMesh(u32 vertCount, Vertex* vertices, u32 indexCount, u32* indices) {
  u32 retInt = m_nextMeshID;
  m_meshStore.emplace(std::pair<u32, WGPUBackendMeshIdx>(retInt, WGPUBackendMeshIdx(m_meshTotalIndices, m_meshTotalVertices, indexCount, vertCount)));
  
  m_meshVertexBuffer.AppendToBack(m_wgpuCore.m_device, m_wgpuQueue, vertices, vertCount);
  m_meshIndexBuffer.AppendToBack(m_wgpuCore.m_device, m_wgpuQueue, indices, indexCount);

  m_nextMeshID++;
  m_meshTotalIndices += indexCount; 
  m_meshTotalVertices += vertCount;

  return retInt;
}

void WGPURenderBackend::DestroyMesh(MeshID meshID) {
  WGPUBackendMeshIdx& gotMesh = m_meshStore[meshID];

  // Removes mesh gpu side informations
  m_meshVertexBuffer.EraseRange(m_wgpuCore.m_device, m_wgpuQueue, gotMesh.m_baseVertex , gotMesh.m_vertexCount);
  m_meshIndexBuffer.EraseRange(m_wgpuCore.m_device, m_wgpuQueue, gotMesh.m_baseIndex , gotMesh.m_indexCount);

  // Readjusts mesh cpu side descriptors
  for (std::pair<MeshID, WGPUBackendMeshIdx> meshIter : m_meshStore) {
    if(meshIter.first > meshID) {
      WGPUBackendMeshIdx& editMesh = meshIter.second;
      editMesh.m_baseIndex -= gotMesh.m_indexCount;
      editMesh.m_baseVertex -= gotMesh.m_vertexCount;
    }
  }

  m_meshTotalVertices -= gotMesh.m_indexCount;
  m_meshTotalVertices -= gotMesh.m_vertexCount;

  // Removes mesh cpu side descriptors
  m_meshStore.erase(meshID);
}

void WGPURenderBackend::RenderUpdate(RenderFrameInfo& state) {
  // Inits frame and checks if frame is able to be rendered
  if (!InitFrame())
  {
      return;
  }

  // >>> Begins processing frame information to be ran by renderer <<<

  // Inserts mesh instance information into a single objData vector
  std::map<MeshID, u32> meshCounts;
  for (MeshRenderInfo meshInstance: state.meshes)
  {
    meshCounts[meshInstance.mesh] += 1;
  }

  u32 totalCount = 0;
  std::unordered_map<MeshID, u32> offsets;
  for (std::pair<MeshID,u32> meshType : meshCounts) 
  {
    offsets[meshType.first] = totalCount;
    totalCount += meshType.second;
  }

  std::vector<WGPUBackendObjectData> objData(totalCount);
  for (MeshRenderInfo meshInstance: state.meshes)
  {
      u32 instanceIdx = offsets[meshInstance.mesh]++;
      glm::mat4x4 normMat = glm::transpose(glm::inverse(meshInstance.matrix));
      objData[instanceIdx] = {meshInstance.matrix, normMat, glm::vec4(meshInstance.rgbColor, 1)};
  }

  // Prepares camera to be rendered through
  float mainCamAspectRatio = (float)m_screenWidth / (float)m_screenHeight;
  glm::mat4x4 mainCamProj = glm::perspective(glm::radians(state.cameraFov), mainCamAspectRatio, state.cameraNear, state.cameraFar);
  glm::mat4x4 mainCamView = state.cameraTransform->GetViewMatrix();
  glm::mat4x4 camSpace = mainCamProj * mainCamView;
    
  // Prepares dynamic shadowed lights to be rendered
  std::vector<glm::mat4x4> dirLightSpaces;
  std::vector<glm::mat4x4> pointLightSpaces;

  // TODO: Make cascade ratios more adjustable
  std::vector<float> cascadeRatios = {0.25, 0.50, 0.75, 1.00};
  const std::vector<WGPUBackendDynamicShadowedDirLightData> shadowedDirLightData = ConvertDirLights(state.dirLights, dirLightSpaces, 4, camSpace, cascadeRatios, 0.05, state.cameraFar);
  const std::vector<WGPUBackendDynamicShadowedPointLightData> shadowedPointLightData = ConvertPointLights(state.pointLights, pointLightSpaces, 1000, 1000);
  const std::vector<WGPUBackendDynamicShadowedSpotLightData> shadowedSpotLightData = ConvertSpotLights(state.spotLights);
  // >>> Actually begins sending off information to be rendered <<<

  // Sends in the attributes of individual mesh instances
  m_instanceDatBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, objData.data(), (u32)objData.size());

  // Begins writing in shadow mapping passes and inserting data for shadowed lights
  for (u8 cascadeIter = 0 ; cascadeIter < DefaultCascadeCount ; cascadeIter++) {
    for (u32 dirShadowIdx = 0 ; dirShadowIdx < shadowedDirLightData.size() ; dirShadowIdx++) {
      m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, dirLightSpaces[dirShadowIdx + cascadeIter * shadowedDirLightData.size()]);
      BeginDepthPass(m_dynamicDirLightShadowMapTexture.GetView(dirShadowIdx * DefaultCascadeCount + cascadeIter));
      DrawObjects(meshCounts);
      EndPass();
    }
  }

  for (u32 pointLightIdx = 0 ; pointLightIdx < shadowedPointLightData.size() ; pointLightIdx++) {
    const WGPUBackendDynamicShadowedPointLightData& pointLight = shadowedPointLightData[pointLightIdx];
    m_fixedPointDepthPassDatBuffer.WriteBuffer(m_wgpuQueue, {pointLight.m_position, pointLight.m_distanceCutoff});
    for (u32 pointShadowIdx = pointLightIdx * 6 ; pointShadowIdx < (pointLightIdx + 1) * 6 ; pointShadowIdx++) {
      m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, pointLightSpaces[pointShadowIdx]);
      BeginPointDepthPass(m_dynamicPointLightShadowMapTexture.GetView(pointShadowIdx));
      DrawObjects(meshCounts);
      EndPass();
    }
  }
  std::cout << "CHECK " << shadowedPointLightData.size() << std::endl;
  // Edits cascade ratios to be put in world space
  float camNearFarDiff = state.cameraFar - state.cameraNear;
  for (float& ratio : cascadeRatios) {
    ratio = state.cameraNear + camNearFarDiff * ratio;
  }
  m_dynamicShadowedDirLightCascadeRatiosBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, cascadeRatios.data(), DefaultCascadeCount);

  // Begins writing in dynamic lights
  m_dynamicShadowedDirLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedDirLightData.data(), (u32)shadowedDirLightData.size());

  m_dynamicShadowedPointLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedPointLightData.data(), (u32)shadowedPointLightData.size());

  m_dynamicShadowedSpotLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedSpotLightData.data(), (u32)shadowedSpotLightData.size());

  m_dynamicShadowLightSpaces.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, dirLightSpaces.data(), (u32)dirLightSpaces.size());

  // Sets fixed data
  WGPUBackendColorPassFixedData colorPassState {
    .m_combined = camSpace,
    .m_view = mainCamView,
    .m_proj = mainCamProj,
    .m_pos = state.cameraTransform->position,
    .m_dirLightCount = (u32)shadowedDirLightData.size(),
    .m_pointLightCount = (u32)shadowedPointLightData.size(),
    .m_spotLightCount = (u32)shadowedSpotLightData.size(),
    .m_dirLightCascadeCount = DefaultCascadeCount
  };
  m_fixedColorPassDatBuffer.WriteBuffer(m_wgpuQueue, colorPassState);

  // Sets the orientation of the view camera for depth pre pass
  m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, camSpace);

  BeginDepthPass(m_depthTexture.m_textureView);
  DrawObjects(meshCounts);
  EndPass();

  BeginColorPass();
  DrawObjects(meshCounts);
  EndPass();

  DrawImGui();
  EndFrame();
}

  // Adds dynamic lights into scene
  LightID WGPURenderBackend::AddDirLight() {
    LightID lightID = m_dynamicShadowedDirLightNextID;
    m_dynamicShadowedDirLightNextID++;
    m_dynamicDirLightShadowMapTexture.RegisterShadow(m_wgpuCore.m_device, m_wgpuQueue);
    return lightID;
  }
  LightID WGPURenderBackend::AddSpotLight() {
    LightID lightID = m_dynamicShadowedSpotLightNextID;
    m_dynamicShadowedSpotLightNextID++;
    return lightID;
  }
  LightID WGPURenderBackend::AddPointLight() {
    LightID lightID = m_dynamicShadowedPointLightNextID;
    m_dynamicShadowedPointLightNextID++;
    m_dynamicPointLightShadowMapTexture.RegisterShadow(m_wgpuCore.m_device, m_wgpuQueue);
    return lightID;
  }

  void WGPURenderBackend::DestroyDirLight(LightID lightID) {
    m_dynamicDirLightShadowMapTexture.UnregisterShadow();
  }
  // TODO Actually implement
  void WGPURenderBackend::DestroySpotLight(LightID lightID) {
    
  }
  void WGPURenderBackend::DestroyPointLight(LightID lightID) {
    m_dynamicPointLightShadowMapTexture.UnregisterShadow();
  }
#pragma endregion
