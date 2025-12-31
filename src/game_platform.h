#pragma once

#include "meta_definitions.h"
#include "renderer/render_types.h"
#include "renderer/render_game.h"
#include "asset_types.h"

#if SKL_INTERNAL
#include "platform_metrics.cpp"
#endif

#include <bitset>   // For ECS
#include <unordered_map>
#include <string>
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

struct GameMemory
{
    
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

// NOTE(marvin): Game platform only needs to know about scene, and only system needs to know about game input. Maybe separate out scene.h?
  
#include "ecs.h"

#define GAME_INITIALIZE(name) void name(Scene &scene, GameMemory &memory, PlatformAPI &platformAPI, bool editor)
typedef GAME_INITIALIZE(game_initialize_t);

#define GAME_UPDATE_AND_RENDER(name) void name(Scene &scene, GameInput &input, f32 deltaTime)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);
