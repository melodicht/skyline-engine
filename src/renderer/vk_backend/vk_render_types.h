// Represents a data buffer stored on the GPU
struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceAddress address;
};

// Represents an image stored on the GPU
struct AllocatedImage
{
    VkImage image;
    VmaAllocation allocation;
};


// Represents a mesh stored on the GPU
struct Mesh
{
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertBuffer;

    u32 indexCount;
};

// Represents a texture stored on the GPU
struct Texture
{
    AllocatedImage texture;
    VkImageView imageView;
    VkExtent2D extent;
    u32 descriptorIndex;
};

// Represents the GPU memory locations of the camera, object, and vertex buffers (CPU->GPU)
struct VertPushConstants
{
    VkDeviceAddress objectAddress;
    VkDeviceAddress vertexAddress;
};

struct VkDirLightData
{
    glm::vec3 direction;
    u32 shadowIndex;

    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct VkSpotLightData
{
    glm::mat4 lightSpace;

    glm::vec3 position;
    glm::vec3 direction;
    u32 shadowIndex;

    glm::vec3 diffuse;
    glm::vec3 specular;

    f32 innerCutoff;
    f32 outerCutoff;
    f32 range;
};

struct VkPointLightData
{
    glm::vec3 position;
    u32 shadowIndex;

    glm::vec3 diffuse;
    glm::vec3 specular;

    f32 constant;
    f32 linear;
    f32 quadratic;

    f32 maxRange;
};

// Represents the direction of the skylight, and the descriptor id of the shadowmap (CPU->GPU)
struct FragPushConstants
{
    VkDeviceAddress dirLightAddress;
    VkDeviceAddress dirCascadeAddress;
    VkDeviceAddress spotLightAddress;
    VkDeviceAddress pointLightAddress;
    u32 dirLightCount;
    u32 dirCascadeCount;
    u32 spotLightCount;
    u32 pointLightCount;
    glm::vec3 ambientLight;
};

struct CubemapPushConstants
{
    glm::vec3 lightPos;
    float farPlane;
};

// Represents the data for a single frame in flight of rendering
struct FrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore acquireSemaphore;
    VkFence renderFence;

    std::vector<AllocatedBuffer> cameraBuffers;
    AllocatedBuffer objectBuffer;
    AllocatedBuffer dirLightBuffer;
    AllocatedBuffer dirCascadeBuffer;
    AllocatedBuffer spotLightBuffer;
    AllocatedBuffer pointLightBuffer;
};

struct LightEntry
{
    u32 cameraIndex;
    Texture shadowMap;
};

// Represents one cascade of a cascaded directional light (CPU->GPU)
struct LightCascade
{
    glm::mat4 lightSpace;
    f32 maxDepth;
};