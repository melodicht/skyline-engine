#include <string>

#include <meta_definitions.h>
#include <asset_types.h>
#include <engine.h>

template <typename T>
s32 WriteFromData(T* dest, DataEntry* data) { return 0; }

template <>
s32 WriteFromData<s32>(s32* dest, DataEntry* data)
{
    if (data->type != INT_ENTRY)
    {
        printf("entry must be int but instead is %d\n", data->type);
        return -1;
    }
    *dest = data->intVal;
    return 0;
}

template <>
s32 WriteFromData<f32>(f32* dest, DataEntry* data)
{
    if (data->type != FLOAT_ENTRY)
    {
        printf("entry must be float but instead is %d\n", data->type);
        return -1;
    }
    *dest = data->floatVal;
    return 0;
}

template <>
s32 WriteFromData<bool>(bool* dest, DataEntry* data)
{
    if (data->type != BOOL_ENTRY)
    {
        printf("entry must be bool but instead is %d\n", data->type);
        return -1;
    }

    *dest = data->boolVal;
    return 0;
}

template <>
s32 WriteFromData<glm::vec3>(glm::vec3* dest, DataEntry* data)
{
    if (data->type != VEC_ENTRY)
    {
        printf("entry must be vec3 but instead is %d\n", data->type);
        return -1;
    }
    *dest = data->vecVal;
    return 0;
}

template <>
s32 WriteFromData<std::string>(std::string* dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        printf("entry must be string but instead is %d\n", data->type);
        return -1;
    }
    *dest = data->stringVal;
    return 0;
}

template <>
s32 WriteFromData<MeshAsset*>(MeshAsset** dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        printf("entry must be string but instead is %d\n", data->type);
        return -1;
    }
    *dest = assetUtils.LoadMeshAsset(data->stringVal);
    return 0;
}

template <>
s32 WriteFromData<TextureAsset*>(TextureAsset** dest, DataEntry* data)
{
    if (data->type != STR_ENTRY)
    {
        printf("entry must be string but instead is %d\n", data->type);
        return -1;
    }
    if (data->stringVal != "")
    {
        *dest = assetUtils.LoadTextureAsset(data->stringVal);
    }
    return 0;
}

template <typename T>
DataEntry* ReadToData(T* src, std::string name)
{
    return new DataEntry(name);
}

template <>
DataEntry* ReadToData<s32>(s32* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<f32>(f32* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<bool>(bool* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<glm::vec3>(glm::vec3* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<std::string>(std::string* src, std::string name)
{
    return new DataEntry(name, *src);
}

template <>
DataEntry* ReadToData<MeshAsset*>(MeshAsset** src, std::string name)
{
    if ((*src) == nullptr)
    {
        return new DataEntry(name, std::string(""));
    }
    return new DataEntry(name, (*src)->name);
}

template <>
DataEntry* ReadToData<TextureAsset*>(TextureAsset** src, std::string name)
{
    if (*src == nullptr)
    {
        return new DataEntry(name, std::string(""));
    }
    return new DataEntry(name, (*src)->name);
}
