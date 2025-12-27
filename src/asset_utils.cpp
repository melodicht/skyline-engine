#include "asset_types.h"
#include "meta_definitions.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <array>

template <>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, f32> {};

template <>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec2, f32> {};

std::unordered_map<std::string, MeshAsset> meshAssets;
std::unordered_map<std::string, TextureAsset> texAssets;

PLATFORM_LOAD_MESH_ASSET(LoadMeshAsset)
{
    if (meshAssets.contains(name))
    {
        return &meshAssets[name];
    }

    std::filesystem::path path = "models/" + name + ".glb";
    fastgltf::Expected<fastgltf::GltfDataBuffer> dataFile = fastgltf::GltfDataBuffer::FromPath(path);
    fastgltf::GltfDataBuffer data;
    if (dataFile)
    {
        data = std::move(dataFile.get());
    }
    else
    {
        return nullptr;
    }

    constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    fastgltf::Expected<fastgltf::Asset> load = parser.loadGltfBinary(data, path.parent_path(), gltfOptions);
    if (load)
    {
        gltf = std::move(load.get());
    }
    else
    {
        return nullptr;
    }

    fastgltf::Mesh mesh = gltf.meshes[0];
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    for (fastgltf::Primitive &p : mesh.primitives)
    {
        fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexAccessor.count);
        fastgltf::iterateAccessor<u32>(gltf, indexAccessor, [&](u32 index)
        {
            indices.push_back(index);
        });

        fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
        fastgltf::Accessor& normAccessor = gltf.accessors[p.findAttribute("NORMAL")->accessorIndex];
        fastgltf::Accessor& uvAccessor = gltf.accessors[p.findAttribute("TEXCOORD_0")->accessorIndex];
        vertices.reserve(vertices.size() + posAccessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 pos, u32 index)
        {
            glm::vec3 norm = fastgltf::getAccessorElement<glm::vec3>(gltf, normAccessor, index);
            glm::vec2 uv = fastgltf::getAccessorElement<glm::vec2>(gltf, uvAccessor, index);

            Vertex vert;
            vert.position = {-pos.z, pos.x, pos.y};
            vert.normal = {-norm.z, norm.x, norm.y};
            vert.uvX = uv.x;
            vert.uvY = uv.y;

            vertices.push_back(vert);
        });
    }

    RenderUploadMeshInfo info{};
    info.vertData = vertices.data();
    info.vertSize = vertices.size();
    info.idxData = indices.data();
    info.idxSize = indices.size();

    MeshAsset asset;
    asset.id = UploadMesh(info);
    meshAssets[name] = asset;

    return &meshAssets[name];
}

struct ImageData {
    u32 width;
    u32 height;
    std::vector<u32> data;
};

ImageData LoadImage(std::filesystem::path path) {
    int width, height, channels;

    stbi_uc* imageData = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    ImageData info{};
    info.data = std::vector<u32>((width * height * 4) / sizeof(u32));

    memcpy(info.data.data(), imageData, info.data.size() * sizeof(u32));
    stbi_image_free(imageData);

    info.width = width;
    info.height = height;
    return info;
}

PLATFORM_LOAD_TEXTURE_ASSET(LoadTextureAsset)
{
    if (texAssets.contains(name))
    {
        return &texAssets[name];
    }

    std::filesystem::path path = "textures/" + name + ".png";
    ImageData info = LoadImage(path);

    TextureAsset asset;
    asset.width = info.width;
    asset.height = info.height;
    RenderUploadTextureInfo uploadInfo ={info.width, info.height, info.data.data()};
    asset.id = UploadTexture(uploadInfo);
    texAssets[name] = asset;
    
    return &texAssets[name];
}

PLATFORM_LOAD_SKYBOX_ASSET(LoadSkyboxAsset)
{
    std::array<std::vector<u32>,6> cubemapData;
    std::filesystem::path firstPath = "textures/" + names[0] + ".png";
    ImageData firstInfo = LoadImage(firstPath);
    u32 firstWidth = firstInfo.width;
    u32 firstHeight = firstInfo.height;
    cubemapData[0] = std::move(firstInfo.data);
    for (u32 i = 1 ; i < 6 ; i ++) {
        std::filesystem::path path = "textures/" + names[i] + ".png";
        ImageData info = LoadImage(path);
        if (info.width != firstWidth || info.height != firstHeight ) {
            LOG_ERROR("Images provided for cubemap do not have uniform dimensions");
            return;
        }
        cubemapData[i] = std::move(info.data);
    }

    std::array<u32*,6> setData;
    for (u32 i = 0 ; i < 6 ; i++) {
        setData[i] = cubemapData[i].data();
    }
    
    RenderSetSkyboxInfo setInfo;
    setInfo.width = firstWidth;
    setInfo.height = firstHeight;
    setInfo.cubemapData = setData;
    SetSkyboxTexture(setInfo);
}
