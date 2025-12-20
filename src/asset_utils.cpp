#include "asset_types.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <toml++/toml.hpp>

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

PLATFORM_LOAD_TEXTURE_ASSET(LoadTextureAsset)
{
    if (texAssets.contains(name))
    {
        return &texAssets[name];
    }

    std::filesystem::path path = "textures/" + name + ".png";

    int width, height, channels;

    stbi_uc* imageData = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    std::vector<u32> pixels;

    pixels = std::vector<u32>((width * height * 4) / sizeof(u32));

    memcpy(pixels.data(), imageData, pixels.size() * sizeof(u32));
    stbi_image_free(imageData);

    RenderUploadTextureInfo info{};
    info.width = width;
    info.height = height;
    info.pixelData = pixels.data();

    TextureAsset asset;
    asset.width = width;
    asset.height = height;
    asset.id = UploadTexture(info);
    texAssets[name] = asset;

    return &texAssets[name];
}

DataEntry* LoadNodeToData(std::string name, toml::node& node);

DataEntry* LoadTableToData(std::string name, toml::table* table)
{
    DataEntry* data = new DataEntry(name);
    for (auto elem : *table)
    {
        data->structVal.push_back(LoadNodeToData(elem.first.data(), elem.second));
    }
    return data;
}

f32 LoadFloatFromNode(toml::node* node)
{
    if (!node->is_floating_point())
    {
        return 0;
    }
    return node->as_floating_point()->get();
}

DataEntry* LoadNodeToData(std::string name, toml::node& node)
{
    if (node.is_integer())
    {
        return new DataEntry(name, (s32)node.as_integer()->get());
    }
    else if (node.is_floating_point())
    {
        return new DataEntry(name, (f32)node.as_floating_point()->get());
    }
    else if (node.is_boolean())
    {
        return new DataEntry(name, node.as_boolean()->get());
    }
    else if (node.is_array())
    {
        toml::array* array = node.as_array();
        glm::vec3 vector =
        {
            LoadFloatFromNode(array->get(0)),
            LoadFloatFromNode(array->get(1)),
            LoadFloatFromNode(array->get(2))
        };
        return new DataEntry(name, vector);
    }
    else if (node.is_string())
    {
        return new DataEntry(name, node.as_string()->get());
    }
    else if (node.is_table())
    {
        return LoadTableToData(name, node.as_table());
    }
    return nullptr;
}

PLATFORM_LOAD_DATA_ASSET(LoadDataAsset)
{
    toml::table file;
    try
    {
        file = toml::parse_file(path);
        return LoadTableToData(name, &file);
    }
    catch (const toml::parse_error& error)
    {
        std::cout << error << '\n';
        return nullptr;
    }
}
