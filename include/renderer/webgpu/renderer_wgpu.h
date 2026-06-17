#pragma once

#include <render_backend.h>

#include <bind_group_wgpu.h>
#include <utils_wgpu.h>
#include <render_types_wgpu.h>
#include <dynamic_shadow_array.h>
#include <single_textures_wgpu.h>
#include <dynamic_light_converter.h>

#include <skl_math_types.h>

#include <webgpu/webgpu.h>

#include <SDL3/SDL.h>

#include <cstdint>
#include <unordered_map>

// Allows for encapsulation of WebGPU render capabilities
class WGPURenderBackend {
private:
    u32 m_screenWidth{ 0 };
    u32 m_screenHeight{ 0 };

    // WGPU objects that remains important throughout rendering 
    // from init to destruction
    WGPUCore m_wgpuCore{ };
    WGPUQueue m_wgpuQueue{ };
    WGPUSurface m_wgpuSurface{ };

    // Stores best supported format on current device
    WGPUTextureFormat m_wgpuTextureFormat{ };
    WGPUTextureFormat m_wgpuDepthTextureFormat{ WGPUTextureFormat_Depth32Float };

    // Represents limits of gpu
    u32 m_defaultArrayMax{ 4096 };
    u32 m_dynamicUniformStrideSize{ 0 };
    b8 m_hardwareDepthClampingSupported = false;

    // Represents temporary variables that are inited/edited/and cleared over the course of frame
    WGPUSurfaceTexture m_surfaceTexture{ };
    WGPUTextureView m_surfaceTextureView{ }; 
    WGPUBackendTexture m_depthTexture{ };

    // Represents the current pass being drawn with
    WGPUCommandEncoder m_passCommandEncoder{ };
    WGPURenderPassEncoder m_renderPassEncoder{ };
    b8 m_renderPassActive{ false };
    b8 m_commandBufferActive{ false };

    // Currently used between color and depth pass
    WGPUBackendBindGroup m_sharedMainViewBindGroup{ };

    // Defines default color pass pipeline
    WGPURenderPipeline m_defaultColorPassPipeline{ };
    WGPUBackendBindGroup m_defaultColorPassBindGroup{ };

    // Defines depth pre pass pipeline
    WGPURenderPipeline m_depthPipeline{ };

    // Defines non-pointlight shadow map
    WGPURenderPipeline m_shadowMapPipeline{ };
    
    WGPUBackendDynamicBindGroup<1> m_dirDepthBindGroup{ };

    // Defines point depth pipeline
    WGPURenderPipeline m_pointDepthPipeline{ };
    WGPUBackendDynamicBindGroup<2> m_pointDepthBindGroup{ };

    // Defines skybox pipeline
    WGPURenderPipeline m_skyboxPipeline{ };
    WGPUBackendBindGroup m_skyboxBindGroup{ };

    // Allocates texture space for shadowmapping based on the amount of shadowed lights registered
    WGPUBackendBaseDynamicShadowMapArray m_dynamicDirLightShadowMapTexture;
    WGPUBackendBaseDynamicShadowMapArray m_dynamicPointLightShadowMapTexture;
    WGPUBackendBaseDynamicShadowMapArray m_dynamicSpotLightShadowMapTexture;

    LightID m_dynamicShadowedDirLightNextID = 0;
    LightID m_dynamicShadowedPointLightNextID = 0;
    LightID m_dynamicShadowedSpotLightNextID = 0;
    
    // Stores GPU buffers
    WGPUBackendSingleUniformBuffer<glm::mat4x4> m_invertedCameraSpaceBuffer{ };
    WGPUBackendSingleUniformBuffer<glm::mat4x4> m_cameraSpaceBuffer{ };
    WGPUBackendSingleStorageArrayBuffer<WGPUBackendObjectData> m_instanceDatBuffer{ };

    WGPUBackendDynamicUniformBuffer<glm::mat4x4> m_pointDepthPassUniformBuffer{ };
    WGPUBackendDynamicUniformBuffer<u32> m_dirDepthPassUniformBuffer{ };

    WGPUBackendSingleUniformBuffer<WGPUBackendColorPassUniforms> m_colorPassUniformBuffer{ };
    WGPUBackendSingleUniformBuffer<WGPUBackendColorPassFixedUniforms> m_colorPassFixedUniformBuffer{ };

    WGPUBackendSingleStorageArrayBuffer<WGPUBackendDynamicShadowedDirLightData> m_dynamicShadowedDirLightBuffer{ };
    WGPUBackendDynamicUniformBuffer<WGPUBackendDynamicShadowedPointLightData> m_dynamicShadowedPointLightBuffer{ };
    WGPUBackendSingleStorageArrayBuffer<WGPUBackendDynamicShadowedSpotLightData> m_dynamicShadowedSpotLightBuffer{ };

    WGPUBackendSingleStorageArrayBuffer<glm::mat4x4> m_dynamicShadowLightSpaces{ };
    WGPUBackendSingleStorageArrayBuffer<float> m_dynamicShadowedDirLightCascadeRatiosBuffer{ };
    WGPUBackendSampler m_shadowMapSampler{ };
    WebGPUBackendCubemapTextureBuffer m_skyboxTexture{ };
    WGPUBackendSampler m_skyboxSampler{ };

    // Vertex buffers
    WGPUBackendVertexArrayBuffer<Vertex> m_meshVertexBuffer{ };
    WGPUBackendIndexArrayBuffer<u32> m_meshIndexBuffer{ };

    u32 m_meshTotalVertices{ 0 };
    u32 m_meshTotalIndices{ 0 };
    // Currently mesh deletion logic requires that meshes with greater MeshID's to correspond to older mesh stores
    std::unordered_map<MeshID, WGPUBackendMeshIdx> m_meshStore{ };

    // The id of the next obj that will be created
    MeshID m_nextMeshID{ 0 }; 

    // Utility objects
    DynamicLightConverter m_lightProcessor{ };

    void ProcessDeviceSpecs();

    // The following getters occur asynchronously in wgpu but is awaited for by these functions
    static WGPUAdapter GetAdapter(const WGPUInstance instance, WGPURequestAdapterOptions const * options);

    static WGPUDevice GetDevice(const WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor);

    // What to call on the queue finishing its work
    static void QueueFinishCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2);

    // What to call on m_wgpuDevice being lost.
    static void LostDeviceCallback(WGPUDevice const * device, WGPUDeviceLostReason reason, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2);

    // What to call on WebGPU error
    static void ErrorCallback(WGPUDevice const * device, WGPUErrorType type, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2);

    // Establishes that the following commands apply to a new frame
    b8 InitFrame();

    // Begins the final color pass that renders frame to color pass
    void BeginColorPass();

    // Begins the skybox pass that renders background of visuals
    void BeginSkyboxPass();

    // Populates depth texture from view of camera buffer
    void BeginDepthPass(WGPUTextureView depthTexture);

    // Populates depth texture from view of orthogonal camera buffer with added biases for dir light
    void BeginDirectionalDepthPass(WGPUTextureView depthTexture, u32 nonPointLightIndex);

    // Populates depth buffer from the view of camera buffer
    void BeginPointDepthPass(WGPUTextureView depthTexture, u32 pointLightIndex, u32 depthPassIndex);

    // Handles some shared code between render passes
    // Currently assumes one pipeline per pass, may need to un-abstract later
    void BeginPass(
        const WGPURenderPassColorAttachment* colorPassAttachment,
        const WGPURenderPassDepthStencilAttachment* depthStencilAttachment,
        std::string&& passLabel,
        const WGPURenderPipeline& pipeline);
    void SetupVandIBO();
    void EndPass();

    // All intermediate passes gets pushed into following command buffers
    void BeginCommandBuffer(
            std::string&& encoderLabel
        );
    void EndCommandBuffer();
    
    
    // Draws engine interface for game if allowed
    void DrawImGui();

    // Takes in mesh counts and renders to current command encoder using previously
    // inserted object data in buffer.
    void DrawObjects(std::map<MeshID, u32>& meshCounts);

    // Ends the current pass and present it to the screen
    void EndFrame();


public:
    // No logic needed
    WGPURenderBackend() { }

    ~WGPURenderBackend();

    // No SDL flags are needed with webgpu
    SDL_WindowFlags GetRenderWindowFlags() { return 0; }

    // Sets a SDL window to draw to and initializes the back end
    void InitRenderer(SDL_Window *window, u32 startWidth, u32 startHeight);

    // Sets up pipelines used to render
    void InitPipelines();

    // Renders and displays frame based on state
    void RenderUpdate(RenderFrameInfo& state);

    // Moves mesh to the GPU, 
    // Returns a uint that represents the mesh's ID
    MeshID UploadMesh(u32 vertCount, Vertex* vertices, u32 indexCount, u32* indices);

    // Removes mesh from GPU and render's mesh ID invalid
    void DestroyMesh(MeshID meshID);

    // Moves set of skybox textures to the GPU
    void SetSkybox(u32 width, u32 height, const std::array<u32*,6>& faceData);

    // Adds dynamic lights into scene
    LightID AddDirLight();
    LightID AddSpotLight();
    LightID AddPointLight();

    // Assures renderer that certain lightId will not longer be used.q
    void DestroyDirLight(LightID lightID);
    void DestroySpotLight(LightID lightID);
    void DestroyPointLight(LightID lightID);
};
