#pragma once

#include <meta_definitions.h>
#include <asset_types.h>

template <typename T> s32 WriteFromData(T* dest, DataEntry* data);
extern template s32 WriteFromData<s32>(s32* dest, DataEntry* data);
extern template s32 WriteFromData<f32>(f32* dest, DataEntry* data);
extern template s32 WriteFromData<bool>(bool* dest, DataEntry* data);
extern template s32 WriteFromData<glm::vec3>(glm::vec3* dest, DataEntry* data);
extern template s32 WriteFromData<std::string>(std::string* dest, DataEntry* data);
extern template s32 WriteFromData<MeshAsset*>(MeshAsset** dest, DataEntry* data);
extern template s32 WriteFromData<TextureAsset*>(TextureAsset** dest, DataEntry* data);

template <typename T> DataEntry* ReadToData(T* src, std::string name);
extern template DataEntry* ReadToData<s32>(s32* src, std::string name);
extern template DataEntry* ReadToData<f32>(f32* src, std::string name);
extern template DataEntry* ReadToData<bool>(bool* src, std::string name);
extern template DataEntry* ReadToData<glm::vec3>(glm::vec3* src, std::string name);
extern template DataEntry* ReadToData<MeshAsset*>(MeshAsset** src, std::string name);
extern template DataEntry* ReadToData<TextureAsset*>(TextureAsset** src, std::string name);