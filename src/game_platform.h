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

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 1200

struct GameInput
{
  f32 mouseDeltaX;
  f32 mouseDeltaY;

  std::set<std::string> keysDown;
};

struct GameMemory
{
    
};

#define PLATFORM_LOAD_MESH_ASSET(proc) MeshAsset* proc(std::string name)
typedef PLATFORM_LOAD_MESH_ASSET(platform_load_mesh_asset_t);

#define PLATFORM_LOAD_TEXTURE_ASSET(proc) TextureAsset* proc(std::string name)
typedef PLATFORM_LOAD_TEXTURE_ASSET(platform_load_texture_asset_t);

#define PLATFORM_LOAD_DATA_ASSET(proc) DataEntry* proc(std::string path)
typedef PLATFORM_LOAD_DATA_ASSET(platform_load_data_asset_t);

#define PLATFORM_WRITE_DATA_ASSET(proc) s32 proc(std::string path, DataEntry* data)
typedef PLATFORM_WRITE_DATA_ASSET(platform_write_data_asset_t);

struct PlatformAPI
{
    // Asset Utility
    platform_load_mesh_asset_t *platformLoadMeshAsset;
    platform_load_texture_asset_t *platformLoadTextureAsset;
    platform_load_data_asset_t *platformLoadDataAsset;
    platform_write_data_asset_t *platformWriteDataAsset;

    // Renderer
    platform_renderer_init_pipelines_t *rendererInitPipelines;
    platform_renderer_add_light_t *rendererAddDirLight;
    platform_renderer_add_light_t *rendererAddSpotLight;
    platform_renderer_add_light_t *rendererAddPointLight;
    platform_renderer_destroy_light_t *rendererDestroyDirLight;
    platform_renderer_destroy_light_t *rendererDestroySpotLight;
    platform_renderer_destroy_light_t *rendererDestroyPointLight;
    platform_renderer_render_update_t *rendererRenderUpdate;
};

// NOTE(marvin): Game platform only needs to know about scene, and only system needs to know about game input. Maybe separate out scene.h?
  
#include "ecs.h"

#define GAME_INITIALIZE(name) void name(Scene &scene, GameMemory &memory, PlatformAPI &platformAPI, bool editor)
typedef GAME_INITIALIZE(game_initialize_t);

#define GAME_UPDATE_AND_RENDER(name) void name(Scene &scene, GameInput &input, f32 deltaTime)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);
