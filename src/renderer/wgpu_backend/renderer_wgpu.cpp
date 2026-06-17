#include <renderer_wgpu.h>
#include <utils_wgpu.h>
#include <sdl3webgpu.h>

#include <meta_definitions.h>
#include <skl_math_utils.h>
#include <wgsl_macro_processor.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <backends/imgui_impl_wgpu.h>

// DEBUG
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

// Much of this was taken from https://eliemichel.github.io/LearnWebGPU

// TODO: Make such constants more configurable
constexpr u32 DefaultCascadeCount = 4;
constexpr u32 DefaultDirLightDim = 1024;
constexpr u32 DefaultPointLightDim = 512;
constexpr u32 DefaultSpotLightDim = 512;
constexpr u32 DefaultSkyboxDim = 2048;


#pragma region Helper Functions
void WGPURenderBackend::ProcessDeviceSpecs() {
  WGPUSupportedFeatures features;
  wgpuDeviceGetFeatures(m_wgpuCore.m_device, &features);

  LOG("Device features:");
  for (s32 iter = 0; iter < features.featureCount ; iter++) {
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

      if (limits.minUniformBufferOffsetAlignment != 256) {
        
      }
  }

  // Gets parameters from limits
  m_dynamicUniformStrideSize = limits.minUniformBufferOffsetAlignment;
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

  ASSERT(requestEnded);

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

  ASSERT(requestEnded);

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
  ImGui_ImplWGPU_NewFrame();

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

// Relies on caller to set bind groups and pipeline since multiple bind groups happen in passes
void WGPURenderBackend::BeginColorPass() {
  WGPURenderPassColorAttachment baseColorPassAttachment {
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

  BeginPass(
    &baseColorPassAttachment,
    &depthStencilAttachment,
    "Color Pass",
    m_defaultColorPassPipeline);
  SetupVandIBO();
}

void WGPURenderBackend::BeginSkyboxPass() {
  WGPURenderPassColorAttachment skyboxPassAttachment {
    .view = m_surfaceTextureView,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .resolveTarget = nullptr,
    .loadOp = WGPULoadOp_Load,
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
  

  BeginPass(
    &skyboxPassAttachment,
    &depthStencilAttachment,
    "Skybox Pass",
    m_skyboxPipeline);
  m_sharedMainViewBindGroup.BindToRenderPass(0, m_renderPassEncoder);
  m_skyboxBindGroup.BindToRenderPass(1, m_renderPassEncoder);
}

void WGPURenderBackend::BeginDepthPass(WGPUTextureView depthTexture) {
  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  BeginPass(
    nullptr,
    &depthStencilAttachment,
    "Depth Pass",
    m_depthPipeline);
  SetupVandIBO();
  m_sharedMainViewBindGroup.BindToRenderPass(0, m_renderPassEncoder);
}

void WGPURenderBackend::BeginDirectionalDepthPass(WGPUTextureView depthTexture, u32 nonPointLightIndex) {
  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };
  BeginPass(
    nullptr,
    &depthStencilAttachment,
    "Directional Depth Pass",
    m_shadowMapPipeline);
  SetupVandIBO();
  m_dirDepthBindGroup.BindToRenderPass(0, m_renderPassEncoder, {nonPointLightIndex});
}

void WGPURenderBackend::BeginPointDepthPass(WGPUTextureView depthTexture, u32 pointLightIndex, u32 depthPassIndex) {
  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  BeginPass(
    nullptr,
    &depthStencilAttachment,
    "Point Depth Pass",
    m_pointDepthPipeline);
  SetupVandIBO();
  m_pointDepthBindGroup.BindToRenderPass(0, m_renderPassEncoder, {depthPassIndex, pointLightIndex});
}

void WGPURenderBackend::BeginPass(
  const WGPURenderPassColorAttachment* colorPassAttachment,
  const WGPURenderPassDepthStencilAttachment* depthStencilAttachment,
  std::string&& passLabel,
  const WGPURenderPipeline& pipeline) {
  // Ensures that no two passes should be active at the same time 
  ASSERT(!m_renderPassActive && m_commandBufferActive);

  m_renderPassActive = true;

  // Sets up pass encoder
  WGPURenderPassDescriptor passDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr(passLabel.c_str()),
    .colorAttachmentCount = colorPassAttachment != nullptr,
    .colorAttachments = colorPassAttachment,
    .depthStencilAttachment = depthStencilAttachment,
    .timestampWrites = nullptr,
  };
  m_renderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_passCommandEncoder, &passDescriptor);

  // Sets render pass as fitting to certain pipeline
  wgpuRenderPassEncoderSetPipeline(m_renderPassEncoder, pipeline);
}

void WGPURenderBackend::SetupVandIBO() {
  m_meshVertexBuffer.BindToRenderPassAsVertexBuffer(m_renderPassEncoder);
  m_meshIndexBuffer.BindToRenderPassAsIndexBuffer(m_renderPassEncoder);
}
void WGPURenderBackend::EndPass() {
  m_renderPassActive = false;

  wgpuRenderPassEncoderEnd(m_renderPassEncoder);
  wgpuRenderPassEncoderRelease(m_renderPassEncoder);
}

void WGPURenderBackend::BeginCommandBuffer(
    std::string&& encoderLabel
  ) {
  ASSERT(!m_commandBufferActive);
  m_commandBufferActive = true;

  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr(encoderLabel.c_str())
  };
  m_passCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);
}

void WGPURenderBackend::EndCommandBuffer() {
  m_commandBufferActive = false;
  WGPUCommandBufferDescriptor cmdBufferDescriptor = {
    .nextInChain = nullptr,
    .label =  WGPUBackendUtils::wgpuStr("Ending pass command buffer"),
  };

  WGPUCommandBuffer passCommand = wgpuCommandEncoderFinish(m_passCommandEncoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(m_passCommandEncoder);

  wgpuQueueSubmit(m_wgpuQueue, 1, &passCommand);
  wgpuCommandBufferRelease(passCommand);
}

// TODO: Incorporate into main render pass
void WGPURenderBackend::DrawImGui() {
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

  LOG("Extracting capabilities...");

  m_hardwareDepthClampingSupported = wgpuAdapterHasFeature(adapter, WGPUFeatureName_DepthClipControl);
  LOG("DEPTH CLAMPING SUPPORTED: " << m_hardwareDepthClampingSupported);

  LOG("Requesting device...");

  // General device description
  WGPULimits deviceRequirements = WGPU_LIMITS_INIT;
  deviceRequirements.maxBindGroups = 2;
  deviceRequirements.maxDynamicUniformBuffersPerPipelineLayout = 1;
  deviceRequirements.maxVertexAttributes = 4;
  deviceRequirements.maxBindingsPerBindGroup = 10;
  deviceRequirements.maxTextureArrayLayers = 2048;

  WGPUFeatureName depthClamping = WGPUFeatureName_DepthClipControl;

  WGPUDeviceDescriptor deviceDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("My Device"),
    .requiredFeatureCount = 1,
    .requiredFeatures = &depthClamping,
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

  ProcessDeviceSpecs();

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
  m_meshVertexBuffer.Init(m_wgpuCore.m_device, "WGPUBackendMeshIdx Vertex Buffer", m_defaultArrayMax);
  m_meshIndexBuffer.Init(m_wgpuCore.m_device, "WGPUBackendMeshIdx Vertex Buffer", m_defaultArrayMax);

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
  ImGui_ImplWGPU_InitInfo imguiInit;
  imguiInit.Device = m_wgpuCore.m_device;
  imguiInit.RenderTargetFormat = m_wgpuTextureFormat;
  imguiInit.DepthStencilFormat = m_wgpuDepthTextureFormat;
  imguiInit.NumFramesInFlight = 3;

  ImGui_ImplWGPU_Init(&imguiInit);

  ImGui_ImplWGPU_NewFrame();
}

WGPUShaderModule loadShader(const WGPUDevice& device, const std::string fileName, const std::string shaderLabel) {
  // Loads in shader module
  size_t loadedDatSize;
  auto loadedDat = SDL_LoadFile(fileName.data(), &loadedDatSize);

  // Makes sure data actually gets loaded in
  ASSERT_PRINT(loadedDat, "Specified shader file doesn't exist");
  
  WGPUShaderSourceWGSL wgslShaderDesc {
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

  // Releases file information
  SDL_free(loadedDat);

  return retModule;
}

// >>> Init helper functionality <<<
// Inserts copy of bind group entry at specific binding
static inline void AddDynamicUniformEntryToBindingGroup(std::vector<WGPUBindGroupLayoutEntry>& bindGroupList, WGPUBindGroupLayoutEntry entry, const u32 binding) {
  entry.binding = binding;
  bindGroupList.push_back(std::move(entry)); 
}
 
static inline WGPUBindGroupLayoutEntry CreateBufferEntry(const WGPUShaderStage visibility, const WGPUBufferBindingType type, const u32 minBindSize) {
  WGPUBindGroupLayoutEntry bindEntry = DefaultBindLayoutEntry();
  bindEntry.visibility = visibility;
  bindEntry.buffer.type = type;
  bindEntry.buffer.minBindingSize = minBindSize;
  return bindEntry;
}

static inline void InsertEntry(std::vector<WGPUBindGroupLayoutEntry>& bindGroupList, WGPUBindGroupLayoutEntry entry, const u32 binding) {
    entry.binding = binding;
    bindGroupList.push_back(std::move(entry)); 
}

static inline WGPUShaderModule InitializeColorShader(const WGPUDevice& device, const std::string fileName, const std::string shaderLabel, const int minDynamicUniformStride) {
  // Loads in shader module
  size_t loadedDatSize;
  auto loadedDat = SDL_LoadFile(fileName.data(), &loadedDatSize);

  // Makes sure data actually gets loaded in
  ASSERT_PRINT(loadedDat, "Given color shader file doesn't exist");
  
  WGPUBackendMacroProcessor colorShaderProcessor(reinterpret_cast<const char *>(loadedDat), static_cast<u64>(loadedDatSize));

  int byteDifference = minDynamicUniformStride - sizeof(WGPUBackendDynamicShadowedPointLightData);

  ASSERT_PRINT(byteDifference >= 0, "WGPUBackendDynamicShadowedPointLightData size is greater than minimum stride size, if this happens you may need to update logic");
  ASSERT_PRINT(byteDifference % 4 == 0, "WGPUBackendDynamicShadowedPointLightData size cannot have integers pad out data");

  int modifiedByteDifference = byteDifference / 4;

  colorShaderProcessor.editIntMacro("POINT_LIGHT_PADDING", modifiedByteDifference);

  WGPUShaderSourceWGSL wgslShaderDesc {
    .chain {
      .next = nullptr,
      .sType = WGPUSType_ShaderSourceWGSL,
    },
    .code{
      .data = colorShaderProcessor.data(),
      .length = colorShaderProcessor.length(),
    },
  };

  WGPUShaderModuleDescriptor shaderDesc {
    .nextInChain = &wgslShaderDesc.chain,
    .label = WGPUBackendUtils::wgpuStr(shaderLabel.data()),
  };

  WGPUShaderModule retModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

  // Releases file information
  SDL_free(loadedDat);

  return retModule;
}

// This function is a bit of a doozy, may want to split this up later
void WGPURenderBackend::InitPipelines()
{
  /**
   * >>> Defines vertex and index buffers used for each pipeline <<<
   */
  std::vector<WGPUVertexAttribute> vertexAttributes{ };
  std::vector<WGPUVertexAttribute> depthVertexAttributes{ };
  std::vector<WGPUVertexAttribute> pointDepthVertexAttributes{ };
  std::vector<WGPUVertexAttribute> skyboxVertexAttributes{ };

  {
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
      .offset =  sizeof(glm::vec3) + sizeof(f32),
      .shaderLocation = 2,
    };
    vertexAttributes.push_back(normVertAttribute);
    depthVertexAttributes.push_back(normVertAttribute);
    pointDepthVertexAttributes.push_back(normVertAttribute);

    WGPUVertexAttribute uvYVertAttribute {
      .nextInChain = nullptr,
      .format = WGPUVertexFormat_Float32,
      .offset =  sizeof(glm::vec3) * 2 + sizeof(f32),
      .shaderLocation = 3,
    };
    vertexAttributes.push_back(uvYVertAttribute);
  }
  
  WGPUVertexBufferLayout bufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(f32) * 2,
    .attributeCount = vertexAttributes.size(),
    .attributes = vertexAttributes.data(),
  };

  WGPUVertexBufferLayout depthBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(f32) * 2,
    .attributeCount = depthVertexAttributes.size(),
    .attributes = depthVertexAttributes.data(),
  };

  WGPUVertexBufferLayout pointDepthBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(f32) * 2,
    .attributeCount = pointDepthVertexAttributes.size(),
    .attributes = pointDepthVertexAttributes.data(),
  };

  WGPUVertexBufferLayout skyboxBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec4),
    .attributeCount = pointDepthVertexAttributes.size(),
    .attributes = pointDepthVertexAttributes.data(),
  };
  
  
  /**
   * >>> Defines bind group layout used for each pipeline <<<
   */
  std::vector<WGPUBindGroupLayoutEntry> sharedBindEntries;
  std::vector<WGPUBindGroupLayoutEntry> colorBindEntries;
  std::vector<WGPUBindGroupLayoutEntry> shadowMapBindEntries;
  std::vector<WGPUBindGroupLayoutEntry> pointDepthBindEntities;
  std::vector<WGPUBindGroupLayoutEntry> skyboxBindEntities;

  {
    // Sets up individual bind group entries
    WGPUBindGroupLayoutEntry cameraSpaceBind = CreateBufferEntry(
      WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      WGPUBufferBindingType_Uniform, sizeof(glm::mat4x4));

    WGPUBindGroupLayoutEntry skyboxInverseSpaceBind = CreateBufferEntry(
      WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      WGPUBufferBindingType_Uniform, sizeof(glm::mat4x4));

    WGPUBindGroupLayoutEntry objDatBind = CreateBufferEntry(
      WGPUShaderStage_Vertex,
      WGPUBufferBindingType_ReadOnlyStorage, sizeof(WGPUBackendObjectData));

    WGPUBindGroupLayoutEntry colorPassUniformBind = CreateBufferEntry(
      WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      WGPUBufferBindingType_Uniform, sizeof(WGPUBackendColorPassUniforms));

    WGPUBindGroupLayoutEntry colorPassFixedUniformBind = CreateBufferEntry(
      WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      WGPUBufferBindingType_Uniform, sizeof(WGPUBackendColorPassFixedUniforms));

    WGPUBindGroupLayoutEntry pointDepthUniformsBind = CreateBufferEntry(
      WGPUShaderStage_Fragment | WGPUShaderStage_Vertex,
      WGPUBufferBindingType_Uniform, sizeof(WGPUBackendPointUniforms));
    pointDepthUniformsBind.buffer.hasDynamicOffset = true;

    WGPUBindGroupLayoutEntry dynamicShadowedDirLightBind = CreateBufferEntry(
      WGPUShaderStage_Fragment,
      WGPUBufferBindingType_ReadOnlyStorage, sizeof(WGPUBackendDynamicShadowedDirLightData));

    WGPUBindGroupLayoutEntry dynamicShadowedSpotLightBind = CreateBufferEntry(
      WGPUShaderStage_Fragment,
      WGPUBufferBindingType_ReadOnlyStorage, sizeof(WGPUBackendDynamicShadowedSpotLightData));

    WGPUBindGroupLayoutEntry dynamicShadowedPointLightBind = CreateBufferEntry(
      WGPUShaderStage_Fragment,
      WGPUBufferBindingType_ReadOnlyStorage, m_dynamicUniformStrideSize);

    WGPUBindGroupLayoutEntry dynamicShadowedPointLightUniformBind = CreateBufferEntry(
      WGPUShaderStage_Fragment,
      WGPUBufferBindingType_Uniform, sizeof(WGPUBackendDynamicShadowedPointLightData));
    dynamicShadowedPointLightUniformBind.buffer.hasDynamicOffset = true;

    // Used by the color pass (fragment) and the shadow map pass (vertex).
    WGPUBindGroupLayoutEntry dynamicShadowLightSpacesBind = CreateBufferEntry(
      WGPUShaderStage_Fragment | WGPUShaderStage_Vertex,
      WGPUBufferBindingType_ReadOnlyStorage, sizeof(glm::mat4x4));

    WGPUBindGroupLayoutEntry dynamicDirLightCascadeRatiosBind = CreateBufferEntry(
      WGPUShaderStage_Fragment,
      WGPUBufferBindingType_ReadOnlyStorage,sizeof(f32));

    WGPUBindGroupLayoutEntry shadowMapUniformsBind = CreateBufferEntry(
      WGPUShaderStage_Vertex,
      WGPUBufferBindingType_Uniform, sizeof(u32));
    shadowMapUniformsBind.buffer.hasDynamicOffset = true;

    WGPUBindGroupLayoutEntry dynamicShadowMapTexturesBind = DefaultBindLayoutEntry();
    dynamicShadowMapTexturesBind.visibility = WGPUShaderStage_Fragment;
    dynamicShadowMapTexturesBind.texture = {
      .nextInChain = nullptr,
      .sampleType = WGPUTextureSampleType_Depth,
      .viewDimension = WGPUTextureViewDimension_2DArray,
      .multisampled = false
    };

    WGPUBindGroupLayoutEntry dynamicPointLightShadowMapBind = DefaultBindLayoutEntry();
    dynamicPointLightShadowMapBind.visibility = WGPUShaderStage_Fragment;
    dynamicPointLightShadowMapBind.texture = {
      .nextInChain = nullptr,
      .sampleType = WGPUTextureSampleType_Depth,
      .viewDimension = WGPUTextureViewDimension_CubeArray,
      .multisampled = false
    };

    WGPUBindGroupLayoutEntry shadowMapSamplerMapBind = DefaultBindLayoutEntry();
    shadowMapSamplerMapBind.visibility = WGPUShaderStage_Fragment;
    shadowMapSamplerMapBind.sampler = {
      .nextInChain = nullptr,
      .type = WGPUSamplerBindingType_Comparison
    };

    WGPUBindGroupLayoutEntry skyboxTextureBind = DefaultBindLayoutEntry();
    skyboxTextureBind.visibility = WGPUShaderStage_Fragment;
    skyboxTextureBind.texture = {
      .nextInChain = nullptr,
      .sampleType = WGPUTextureSampleType_Float,
      .viewDimension = WGPUTextureViewDimension_Cube,
      .multisampled = false
    };

    WGPUBindGroupLayoutEntry skyboxSamplerBind = DefaultBindLayoutEntry();
    skyboxSamplerBind.visibility = WGPUShaderStage_Fragment;
    skyboxSamplerBind.sampler = {
      .nextInChain = nullptr,
      .type = WGPUSamplerBindingType_NonFiltering
    };

    // Gathers defined layout entries into groups.
    InsertEntry(sharedBindEntries, cameraSpaceBind, 0);
    InsertEntry(sharedBindEntries, objDatBind, 1);

    InsertEntry(colorBindEntries, colorPassUniformBind,0);
    InsertEntry(colorBindEntries, colorPassFixedUniformBind,1);
    InsertEntry(colorBindEntries, dynamicShadowedDirLightBind, 2);
    InsertEntry(colorBindEntries, dynamicShadowedPointLightBind, 3);
    InsertEntry(colorBindEntries, dynamicShadowedSpotLightBind, 4);
    InsertEntry(colorBindEntries, dynamicShadowLightSpacesBind, 5);
    InsertEntry(colorBindEntries, dynamicShadowMapTexturesBind, 6);
    InsertEntry(colorBindEntries, dynamicShadowMapTexturesBind, 7);
    InsertEntry(colorBindEntries, dynamicPointLightShadowMapBind, 8);
    InsertEntry(colorBindEntries, dynamicDirLightCascadeRatiosBind, 9);   
    InsertEntry(colorBindEntries, shadowMapSamplerMapBind, 10);

    InsertEntry(shadowMapBindEntries, shadowMapUniformsBind, 0);
    InsertEntry(shadowMapBindEntries, dynamicShadowLightSpacesBind, 1);
    InsertEntry(shadowMapBindEntries, objDatBind, 2);

    InsertEntry(pointDepthBindEntities, pointDepthUniformsBind, 0);
    InsertEntry(pointDepthBindEntities, dynamicShadowedPointLightUniformBind, 1);
    InsertEntry(pointDepthBindEntities, objDatBind, 2);

    InsertEntry(skyboxBindEntities, skyboxTextureBind, 0);
    InsertEntry(skyboxBindEntities, skyboxSamplerBind, 1);
    InsertEntry(skyboxBindEntities, cameraSpaceBind, 2);
  }
  
  WGPUBindGroupLayoutDescriptor sharedBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Shared Bind Layout"),
    .entryCount = sharedBindEntries.size(), 
    .entries = sharedBindEntries.data(),
  };

  WGPUBindGroupLayoutDescriptor colorPassBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Bind Layout"),
    .entryCount = colorBindEntries.size(), 
    .entries = colorBindEntries.data(),
  };

  WGPUBindGroupLayoutDescriptor pointDepthBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pass Bind Layout"),
    .entryCount = pointDepthBindEntities.size(),
    .entries = pointDepthBindEntities.data(),
  };

  WGPUBindGroupLayoutDescriptor shadowMapBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Shadow Mapping Pass Bind Layout"),
    .entryCount = shadowMapBindEntries.size(),
    .entries = shadowMapBindEntries.data(),
  };

  WGPUBindGroupLayoutDescriptor skyboxBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Skybox Pass Bind Layout"),
    .entryCount = skyboxBindEntities.size(),
    .entries = skyboxBindEntities.data(),
  };


  /**
   * >>> Constructs bind group layouts on WebGpu <<<
   */
  WGPUGuardedBindGroupLayout sharedLayout {wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &sharedBindLayoutDescriptor)};
  
  WGPUGuardedBindGroupLayout colorPassBindLayout {wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &colorPassBindLayoutDescriptor)};
  std::array<WGPUBindGroupLayout,2> colorLayouts = {sharedLayout.get(), colorPassBindLayout.get()};

  WGPUGuardedBindGroupLayout pointDepthBindLayout {wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &pointDepthBindLayoutDescriptor)};

  WGPUGuardedBindGroupLayout shadowMapBindLayout{wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &shadowMapBindLayoutDescriptor)};

  WGPUGuardedBindGroupLayout skyboxBindLayout{wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &skyboxBindLayoutDescriptor)};
  std::array<WGPUBindGroupLayout,2> skyboxLayouts = {skyboxBindLayout.get()};


  /** 
   * >>> Prepares bind group layouts for individual pipelines <<<
   */
  WGPUPipelineLayoutDescriptor pipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Pipeline Layout"),
    .bindGroupLayoutCount = 2,
    .bindGroupLayouts = colorLayouts.data(),
  };

  WGPUPipelineLayoutDescriptor pointDepthPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = pointDepthBindLayout.data(),
  };

  WGPUPipelineLayoutDescriptor shadowMapPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Shadow Map Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = shadowMapBindLayout.data()
  };

  WGPUPipelineLayoutDescriptor depthPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = sharedLayout.data(),
  };

  WGPUPipelineLayoutDescriptor skyboxPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Skybox Pipeline Layout"),
    .bindGroupLayoutCount = 2,
    .bindGroupLayouts = skyboxLayouts.data(),
  };

  WGPUGuardedPipelineLayout pipelineLayout {wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &pipelineLayoutConstructor)};
  WGPUGuardedPipelineLayout pointDepthPipelineLayout {wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &pointDepthPipelineLayoutConstructor)};
  WGPUGuardedPipelineLayout shadowMapPipelineLayout {wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &shadowMapPipelineLayoutConstructor)};
  WGPUGuardedPipelineLayout depthPipelineLayout {wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &depthPipelineLayoutConstructor)};
  WGPUGuardedPipelineLayout skyboxPipelineLayout {wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &skyboxPipelineLayoutConstructor)};


  /**
   * >>> Creates pipeline <<< 
   * Combines previously defined layouts, shader data, and shader state descriptors
   * to create pipeline.
   */
  {
    // Gathers shader code from file
    // TODO: Eventually we would want to find some abstraction from file system actually guarantee such shaders would be there without crash.
    WGPUGuardedShaderModule colorShaderModule {InitializeColorShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/color_shader.wgsl", "Color Pass Shader", m_dynamicUniformStrideSize)};

    WGPUGuardedShaderModule pointDepthShaderModule {loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/point_depth_shader.wgsl", "Point Depth Pass Shader")};

    WGPUGuardedShaderModule shadowMapShaderModule {loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/shadow_map.wgsl", "Shadow Mapping Pass Shader")};

    WGPUGuardedShaderModule depthShaderModule {loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/depth_shader.wgsl", "Depth Pass Shader")};

    WGPUGuardedShaderModule skyboxShaderModule {loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/skybox_shader.wgsl", "Skybox Pass Shader")};

    // Packages shader states for each pipeline
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
    
    WGPUDepthStencilState depthStencilSetSlopeBiased = depthStencilState;
    depthStencilSetSlopeBiased.depthBias = 2;
    depthStencilSetSlopeBiased.depthBiasSlopeScale = 4.5f; // TODO: Make less arbitrary allow to pass in

    WGPUDepthStencilState depthStencilReadOnlyState = depthStencilState;
    depthStencilReadOnlyState.depthWriteEnabled = WGPUOptionalBool_False;
    
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
      .module = colorShaderModule.get(),
      .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = 1,
      .targets = &colorTarget,
    };

    WGPUFragmentState pointDepthDummyFragState {
      .module = pointDepthShaderModule.get(),
      .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = 0,
      .targets = nullptr,
    };

    WGPUFragmentState skyboxFragState {
      .module = skyboxShaderModule.get(),
      .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = 1,
      .targets = &colorTarget,
    };
    
    // Gathers previous information into descriptors and establish 
    WGPURenderPipelineDescriptor skyboxPipelineDesc {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Skybox Pipeline"),
      .layout = skyboxPipelineLayout.get(),
      .vertex {
        .module = skyboxShaderModule.get(),
        .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
        .constantCount = 0,
        .constants = nullptr,
        .bufferCount = 0,
        .buffers = &skyboxBufferLayout
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
      .fragment = &skyboxFragState,
    };

    WGPURenderPipelineDescriptor depthPipelineDesc {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Depth Pipeline"),
      .layout = depthPipelineLayout.get(),
      .vertex {
        .module = depthShaderModule.get(),
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

    // Ideally ensures that depth clamping logic still goes through
    // even when not supported by hardware
    WGPUConstantEntry enableManualClampingEntry {
      .key = WGPUBackendUtils::wgpuStr("manualClamping"),
      .value = static_cast<double>(!m_hardwareDepthClampingSupported)
    };

    WGPURenderPipelineDescriptor shadowmapPipelineDesc {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Shadow Mapping Pipeline"),
      .layout = shadowMapPipelineLayout.get(),
      .vertex {
        .module = shadowMapShaderModule.get(),
        .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
        .constantCount = 1,
        .constants = &enableManualClampingEntry,
        .bufferCount = 1,
        .buffers = &depthBufferLayout
      },
      .primitive {
        .topology = WGPUPrimitiveTopology_TriangleList,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_Back,
        .unclippedDepth = m_hardwareDepthClampingSupported
      },
      .depthStencil = &depthStencilSetSlopeBiased,
      .multisample {
        .count = 1,
        .mask = ~0u,
        .alphaToCoverageEnabled = false,
      },
      .fragment = nullptr,
    };

    WGPURenderPipelineDescriptor pointDepthPipelineDesc {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Point Depth Pipeline "),
      .layout = pointDepthPipelineLayout.get(),
      .vertex {
        .module = pointDepthShaderModule.get(),
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

    WGPURenderPipelineDescriptor pipelineDesc {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Color Pipeline"),
      .layout = pipelineLayout.get(),
      .vertex {
        .module = colorShaderModule.get(),
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

    // Gathers pipelines into global pipeline variables
    m_defaultColorPassPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &pipelineDesc);
    m_depthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &depthPipelineDesc);
    m_shadowMapPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &shadowmapPipelineDesc);
    m_pointDepthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &pointDepthPipelineDesc);
    m_skyboxPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &skyboxPipelineDesc); 
  }
  

  /**
   * >>> Initializes all gpu buffers <<<
   */
  {
    m_invertedCameraSpaceBuffer.Init(m_wgpuCore.m_device, "Inverted Camera Space Buffer");
    m_cameraSpaceBuffer.Init(m_wgpuCore.m_device, "Camera Space Buffer");
    m_instanceDatBuffer.Init(m_wgpuCore.m_device, "Instance Buffer", m_defaultArrayMax);

    m_colorPassUniformBuffer.Init(m_wgpuCore.m_device, "Color Pass Uniforms Buffer");
    m_colorPassFixedUniformBuffer.Init(m_wgpuCore.m_device, "Color Pass Fixed Uniforms Buffer");
    m_dynamicShadowedDirLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Direction Light Buffer", m_defaultArrayMax);
    m_dynamicShadowedPointLightBuffer.Init(m_wgpuCore.m_device, m_dynamicUniformStrideSize, "Dynamic Shadowed Point Light Buffer", m_defaultArrayMax);
    m_dynamicShadowedSpotLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Dir Light Buffer", m_defaultArrayMax);
    m_dynamicShadowLightSpaces.Init(m_wgpuCore.m_device, "Dynamic Shadow Light Spaces", m_defaultArrayMax);
    m_dynamicShadowedDirLightCascadeRatiosBuffer.Init(m_wgpuCore.m_device, "Shadowed Dynamic Directional Light Cascade Ratios", DefaultCascadeCount);

    m_pointDepthPassUniformBuffer.Init(m_wgpuCore.m_device, m_dynamicUniformStrideSize, "Point Depth Pass Uniform Data Buffer", m_defaultArrayMax);

    m_dirDepthPassUniformBuffer.Init(m_wgpuCore.m_device, m_dynamicUniformStrideSize, "Directional Depth Pass Uniform Data Buffer", m_defaultArrayMax);
    
    m_dynamicDirLightShadowMapTexture.Init(
      m_wgpuCore.m_device, 
      DefaultDirLightDim, 
      DefaultDirLightDim, 
      32, 
      DefaultCascadeCount, 
      "Dynamic Direction Light Shadow Maps", 
      "Dynamic Direction Light Shadow Maps Whole", 
      "Dynamic Direction Light Shadow Maps Layer",
      false);
    m_dynamicPointLightShadowMapTexture.Init(
      m_wgpuCore.m_device, 
      DefaultPointLightDim, 
      DefaultPointLightDim, 
      2048, 
      6,
      "Dynamic Point Light Shadow Maps", 
      "Dynamic Point Light Shadow Maps Cube Arrays", 
      "Dynamic Point Light Shadow Maps Texture Layer",
      true);
    m_dynamicSpotLightShadowMapTexture.Init(
      m_wgpuCore.m_device, 
      DefaultSpotLightDim, 
      DefaultSpotLightDim, 
      2048, 
      1,
      "Dynamic Spot Light Shadow Maps", 
      "Dynamic Spot Light Shadow Maps Cube Arrays", 
      "Dynamic Spt Light Shadow Maps Texture Layer",
      false);
    m_shadowMapSampler.InitOrUpdate(
      m_wgpuCore.m_device, 
      m_wgpuQueue, 
      WGPUAddressMode_ClampToEdge, 
      WGPUFilterMode_Linear, 
      WGPUFilterMode_Linear, 
      WGPUMipmapFilterMode_Nearest, 
      0.0, 
      0.0, 
      WGPUCompareFunction_Less, 
      1, 
      "Shadow Map Sampler");
    m_skyboxTexture.Init(
      m_wgpuCore.m_device,
      DefaultSkyboxDim,
      DefaultSkyboxDim,
      "Skybox Texture",
      "Skybox Texture Cubemap View"
    );
    m_skyboxSampler.InitOrUpdate(
      m_wgpuCore.m_device, 
      m_wgpuQueue, 
      WGPUAddressMode_ClampToEdge, 
      WGPUFilterMode_Nearest, 
      WGPUFilterMode_Nearest, 
      WGPUMipmapFilterMode_Nearest, 
      0.0, 
      0.0, 
      WGPUCompareFunction_Undefined, 
      1, 
      "Skybox Sampler"
    );
  }


  /**
   * >>> Initializes all bind groups <<<
   */
  {
    m_sharedMainViewBindGroup.Init("Shared Pipeline Pass Bind Group", sharedLayout.get(), m_dynamicUniformStrideSize);
    m_dirDepthBindGroup.Init("Depth Pass Pipeline Bind Group", shadowMapBindLayout.get(), m_dynamicUniformStrideSize);
    m_defaultColorPassBindGroup.Init("Color Pass Pipeline Bind Group", colorPassBindLayout.get(), m_dynamicUniformStrideSize);
    m_pointDepthBindGroup.Init("Point Depth Pass Bind Group", pointDepthBindLayout.get(), m_dynamicUniformStrideSize);
    m_skyboxBindGroup.Init("Skybox pass bind group", skyboxBindLayout.get(), m_dynamicUniformStrideSize);
  }

   
  /**
   * >>> Attaches relevant gpu buffers to bind groups <<<
   */
  {
    m_cameraSpaceBuffer.RegisterBindGroup(&m_sharedMainViewBindGroup, 0);
    m_instanceDatBuffer.RegisterBindGroup(&m_sharedMainViewBindGroup, 1);

    m_colorPassUniformBuffer.RegisterBindGroup(&m_defaultColorPassBindGroup, 0);
    m_colorPassFixedUniformBuffer.RegisterBindGroup(&m_defaultColorPassBindGroup, 1);
    m_dynamicShadowedDirLightBuffer.RegisterBindGroup(&m_defaultColorPassBindGroup, 2);
    m_dynamicShadowedPointLightBuffer.RegisterBindGroup(&m_defaultColorPassBindGroup, 3);
    m_dynamicShadowedSpotLightBuffer.RegisterBindGroup(&m_defaultColorPassBindGroup, 4);
    m_dynamicShadowLightSpaces.RegisterBindGroup(&m_defaultColorPassBindGroup, 5);
    m_dynamicDirLightShadowMapTexture.RegisterBindGroup(&m_defaultColorPassBindGroup, 6);
    m_dynamicSpotLightShadowMapTexture.RegisterBindGroup(&m_defaultColorPassBindGroup, 7);
    m_dynamicPointLightShadowMapTexture.RegisterBindGroup(&m_defaultColorPassBindGroup, 8);
    m_dynamicShadowedDirLightCascadeRatiosBuffer.RegisterBindGroup(&m_defaultColorPassBindGroup, 9);
    m_shadowMapSampler.RegisterBindGroup(&m_defaultColorPassBindGroup, 10);

    m_pointDepthPassUniformBuffer.RegisterBindGroupAsDynamicUniform(&m_pointDepthBindGroup, 0);
    m_dynamicShadowedPointLightBuffer.RegisterBindGroupAsDynamicUniform(&m_pointDepthBindGroup, 1);
    m_instanceDatBuffer.RegisterBindGroup(&m_pointDepthBindGroup, 2);

    m_dirDepthPassUniformBuffer.RegisterBindGroupAsDynamicUniform(&m_dirDepthBindGroup, 0);
    m_dynamicShadowLightSpaces.RegisterBindGroup(&m_dirDepthBindGroup, 1);
    m_instanceDatBuffer.RegisterBindGroup(&m_dirDepthBindGroup, 2);
    
    m_skyboxTexture.RegisterBindGroup(&m_skyboxBindGroup, 0);
    m_skyboxSampler.RegisterBindGroup(&m_skyboxBindGroup, 1);
    m_invertedCameraSpaceBuffer.RegisterBindGroup(&m_skyboxBindGroup, 2);
    
    m_defaultColorPassBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
    m_sharedMainViewBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
    m_pointDepthBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
    m_skyboxBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  }

  // Initializes fixed uniform buffers
  WGPUBackendColorPassFixedUniforms uniforms {
    .m_dirLightCascadeCount = DefaultCascadeCount,
    .m_dirLightMapPixelDimension = DefaultDirLightDim,
    .m_pointLightMapPixelDimension = DefaultPointLightDim,
    .m_pcfRange = 2
  };
  m_colorPassFixedUniformBuffer.WriteBuffer(m_wgpuQueue, uniforms);

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

void WGPURenderBackend::SetSkybox(u32 width, u32 height, const std::array<u32*,6>& faceData) {
  m_skyboxTexture.Insert(m_wgpuCore.m_device, m_wgpuQueue, width, height, faceData);
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
  f32 mainCamAspectRatio = (f32)m_screenWidth / (f32)m_screenHeight;
  glm::mat4x4 mainCamProj = glm::perspective(glm::radians(state.cameraFov), mainCamAspectRatio, state.cameraNear, state.cameraFar);
  glm::mat4x4 mainCamView = state.cameraTransform->GetViewMatrix();
  glm::mat4x4 camSpace = mainCamProj * mainCamView;
    
  // Prepares dynamic shadowed lights to be rendered
  std::vector<glm::mat4x4> dirLightSpaces;
  std::vector<glm::mat4x4> pointLightSpaces;

  // TODO: Make cascade ratios more adjustable
  std::vector<f32> cascadeRatios = {0.10, 0.25, 0.50, 1.00};
  const std::vector<WGPUBackendDynamicShadowedDirLightData> shadowedDirLightData = 
    m_lightProcessor.ConvertDirLights(
      state.dirLights, 
      dirLightSpaces, 
      mainCamProj, 
      mainCamView,
      cascadeRatios,
      0.05,
      DefaultDirLightDim,
      state.cameraNear,
      state.cameraFar);
  const std::vector<WGPUBackendDynamicShadowedPointLightData> shadowedPointLightData = 
    m_lightProcessor.ConvertPointLights(state.pointLights, pointLightSpaces, DefaultPointLightDim, DefaultPointLightDim);
  const std::vector<WGPUBackendDynamicShadowedSpotLightData> shadowedSpotLightData = 
    m_lightProcessor.ConvertSpotLights(state.spotLights);

  // >>> Actually begins sending off information to be rendered <<<
  // Sends in the attributes of individual mesh instances
  m_instanceDatBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, objData.data(), (u32)objData.size());

  // Edits cascade ratios to be put in world space
  f32 camNearFarDiff = state.cameraFar - state.cameraNear;
  for (f32& ratio : cascadeRatios) {
    ratio = state.cameraNear + camNearFarDiff * ratio;
  }
  m_dynamicShadowedDirLightCascadeRatiosBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, cascadeRatios.data(), DefaultCascadeCount);

  // Begins writing in dynamic lights
  m_dynamicShadowedDirLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedDirLightData);

  m_dynamicShadowedPointLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedPointLightData);

  m_dynamicShadowedSpotLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedSpotLightData);

  m_dynamicShadowLightSpaces.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, dirLightSpaces);

  // Sets uniform data
  WGPUBackendColorPassUniforms colorPassState {
    .m_view = mainCamView,
    .m_pos = state.cameraTransform->GetWorldPosition(),
    .m_dirLightCount = (u32)shadowedDirLightData.size(),
    .m_pointLightCount = (u32)shadowedPointLightData.size(),
    .m_spotLightCount = (u32)shadowedSpotLightData.size(),
    .m_dirLightCascadeCount = DefaultCascadeCount,
    .m_dirLightMapPixelDimension = DefaultDirLightDim,
    .m_pointLightMapPixelDimension = DefaultPointLightDim
  };
  m_colorPassUniformBuffer.WriteBuffer(m_wgpuQueue, colorPassState);

  // Writes inverted camera space
  glm::mat4x4 rotAndProjInverse = glm::inverse(mainCamProj * glm::mat4x4(glm::mat3x3(mainCamView)));
  m_invertedCameraSpaceBuffer.WriteBuffer(
    m_wgpuQueue,
    rotAndProjInverse);
  // Writes to main view of into cam space
  m_cameraSpaceBuffer.WriteBuffer(
    m_wgpuQueue,
    camSpace); 

  // Writes in data for shadow mapping
  m_pointDepthPassUniformBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, pointLightSpaces);
  

  // One index per (light, cascade) light-space matrix. The shadow pass selects a slot via
  // dynamic offset and the shader uses its value to index cameraSpaces, so slot j must hold j
  // across the full cascadeCount * dirLightCount range (matches dirLightSpaces).
  std::vector<u32> dirIndices;
  dirIndices.reserve(dirLightSpaces.size());
  for (u32 i = 0 ; i < dirLightSpaces.size() ; i++) {
    dirIndices.push_back(i);
  }
  m_dirDepthPassUniformBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, dirIndices);

  // >>> Updates Dirty Binding Groups <<<

  m_sharedMainViewBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_pointDepthBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_skyboxBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_defaultColorPassBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_dirDepthBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
    
  // >>> Begins making draw calls <<<

  for (u32 pointLightIdx = 0 ; pointLightIdx < shadowedPointLightData.size() ; pointLightIdx += 1) {
    const WGPUBackendDynamicShadowedPointLightData& pointLight = shadowedPointLightData[pointLightIdx];
    BeginCommandBuffer("Point Light Pass");
    for (u32 pointShadowIdx = pointLightIdx * 6 ; pointShadowIdx < (pointLightIdx + 1) * 6 ; pointShadowIdx += 1) {
      BeginPointDepthPass(m_dynamicPointLightShadowMapTexture.GetView(pointShadowIdx), pointLightIdx, pointShadowIdx);
      DrawObjects(meshCounts);
      EndPass();
    }
    EndCommandBuffer();
  }

  BeginCommandBuffer("Pre-Color Command Encoder");
  // Begins writing in shadow mapping passes and inserting data for shadowed lights

  for (u8 cascadeIter = 0 ; cascadeIter < DefaultCascadeCount ; cascadeIter += 1) {
    for (u32 dirShadowIdx = 0 ; dirShadowIdx < shadowedDirLightData.size() ; dirShadowIdx += 1) {
      u32 dirLightSpaceIndex = dirShadowIdx + cascadeIter * shadowedDirLightData.size();
      BeginDirectionalDepthPass(m_dynamicDirLightShadowMapTexture.GetView(dirShadowIdx * DefaultCascadeCount + cascadeIter), dirLightSpaceIndex);
      DrawObjects(meshCounts);
      EndPass();
    }
  }

  u32 spotShadowSpaceIdx = DefaultCascadeCount * shadowedDirLightData.size();
  for (u32 spotShadowIter = 0 ; spotShadowIter <  shadowedSpotLightData.size() ; spotShadowIter += 1) {
    BeginDirectionalDepthPass(m_dynamicDirLightShadowMapTexture.GetView(spotShadowIter), spotShadowSpaceIdx);
    DrawObjects(meshCounts);
    EndPass();
    spotShadowSpaceIdx += 1;
  }

  // Sets the orientation of the view camera for depth pre pass
  BeginDepthPass(m_depthTexture.m_textureView);
  DrawObjects(meshCounts);
  EndPass();
  EndCommandBuffer();

  BeginCommandBuffer("Color Command Encoder");
  BeginColorPass();
  
  // Draw main objects
  m_sharedMainViewBindGroup.BindToRenderPass(0, m_renderPassEncoder);
  m_defaultColorPassBindGroup.BindToRenderPass(1, m_renderPassEncoder);
  DrawObjects(meshCounts);

  wgpuRenderPassEncoderSetPipeline(m_renderPassEncoder, m_skyboxPipeline);
  m_skyboxBindGroup.BindToRenderPass(0, m_renderPassEncoder);
  wgpuRenderPassEncoderDraw(m_renderPassEncoder, 3, 1, 0, 0);

  EndPass();  
  EndCommandBuffer();

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
  m_dynamicSpotLightShadowMapTexture.RegisterShadow(m_wgpuCore.m_device, m_wgpuQueue);
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
  m_dynamicSpotLightShadowMapTexture.UnregisterShadow();
}
void WGPURenderBackend::DestroyPointLight(LightID lightID) {
  m_dynamicPointLightShadowMapTexture.UnregisterShadow();
}
#pragma endregion
