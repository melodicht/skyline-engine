#pragma once

#include "renderer/render_types.h"
#include "renderer/render_game.h"
#include "asset_types.h"

#if SKL_INTERNAL
#include "platform_metrics.cpp"
#endif

#if SKL_ENABLED_EDITOR
#include <imgui.h>
#endif

#include <bitset>   // For ECS
#include <unordered_map>
#include <string>
#include <cstring>
#include <set>
#include <array>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 1200

struct GameInput
{
    s32 mouseDeltaX;
    s32 mouseDeltaY;
    u32 mouseX;
    u32 mouseY;

    std::set<std::string> keysDown;
};

#define ASSET_UTIL_FUNCS(method)\
    method(MeshAsset*,LoadMeshAsset,(std::string name))\
    method(TextureAsset*,LoadTextureAsset,(std::string name))\
    method(void,LoadSkyboxAsset,(std::array<std::string,6> names))\
    method(DataEntry*,LoadDataAsset,(std::string path))\
    method(s32,WriteDataAsset,(std::string path, DataEntry* data))
DEFINE_GAME_MODULE_API(PlatformAssetUtils,ASSET_UTIL_FUNCS)

struct PlatformAPI
{
    // Asset Utility
    PlatformAssetUtils assetUtils;

    // Renderer
    PlatformRenderer renderer;
};

struct GameMemory
{
    u64 permanentStorageSize;  // In bytes
    void *permanentStorage;

    // TODO(marvin): Does the imgui context really belong to game memory? Should it be part of the debug storage?
    ImGuiContext *imGuiContext;

#if SKL_INTERNAL
    u64 debugStorageSize;  // In bytes
    void *debugStorage;
#endif

    PlatformAPI platformAPI;
};


// NOTE(marvin): Initialize is anything has to be done at the start ONCE, whereas load is anything that has to be done after the game module is loaded in, including after hot reloads.

#define GAME_INITIALIZE(name) void name(GameMemory &memory, std::string mapName, bool editor)
typedef GAME_INITIALIZE(game_initialize_t);

#define GAME_LOAD(name) void name(GameMemory &memory)
typedef GAME_LOAD(game_load_t);

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory &memory, GameInput &input, f32 deltaTime)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);
