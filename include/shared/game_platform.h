#pragma once

#include <set>
#include <string>

#include <meta_definitions.h>
#include <asset_types.h>
#include <render_game.h>

struct GameInput
{
    s32 mouseDeltaX;
    s32 mouseDeltaY;
    u32 mouseX;
    u32 mouseY;

    std::set<std::string> keysDown;
};

#define ASSET_UTIL_FUNCS(method)\
    method(MeshAsset *,LoadMeshAsset,(std::string name))\
    method(TextureAsset*,LoadTextureAsset,(std::string name))\
    method(void,LoadSkyboxAsset,(std::array<std::string,6> names))\
    method(DataEntry*,LoadDataAsset,(std::string name))\
    method(s32,WriteDataAsset,(std::string name, DataEntry* data))
DEFINE_GAME_MODULE_API(PlatformAssetUtils,ASSET_UTIL_FUNCS)

// TODO(marvin): The creation of the SKL Jolt Allocator must happen on the platform side so that the virtual table can survive the hot reload. I don't see a better way than this...

#define ALLOCATOR_FUNCS(method) \
    method(void *,AlignedAllocate,(siz size, siz alignment)) \
    method(void,AlignedFree,(void *block)) \
    method(void *,Allocate,(siz size)) \
    method(void,Free,(void *block)) \
    method(void *,Realloc,(void *block, siz oldSize, siz newSize))
DEFINE_GAME_MODULE_API(PlatformAllocator, ALLOCATOR_FUNCS)

struct PlatformAPI
{
    PlatformAssetUtils assetUtils;
    PlatformRenderer renderer;
    PlatformAllocator allocator;
};

struct ImGuiContext;
struct DebugState;

struct GameMemory
{
    void* fixedSizeStorage;

    // TODO(marvin): Does the imgui context really belong to game memory? Should it be part of the debug storage?
    ImGuiContext *imGuiContext;

#if SKL_INTERNAL
    void* debugStorage;
    DebugState* debugState;
#endif

    PlatformAPI platformAPI;

    // NOTE(marvin): It is contained in the scene as well, but the SKL physics system's temp allocator need to be reset due to vtable x hot reload conflict. Putting it here for convenience.
    void* sklPhysicsSystem;
};


// NOTE(marvin): Load is for setting up infrastructure, anything that
// has to be done after the game module is loaded in, including after
// hot reloads, whereas initialize is anything has to be done at the
// start ONCE. Thus, load happens before game initialize on game boot,
// and also before game update and render on hot reload (obviously).

#define GAME_LOAD(name) void name(GameMemory &memory, b32 editor, b32 gameInitialized)
typedef GAME_LOAD(game_load_t);

#define GAME_INITIALIZE(name) void name(GameMemory &memory, std::string mapName, b32 editor)
typedef GAME_INITIALIZE(game_initialize_t);

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory &memory, GameInput &input, f32 deltaTime)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);
