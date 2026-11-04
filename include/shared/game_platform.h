#pragma once

#include <string>
#include <vector>

#include <meta_definitions.h>
#include <asset_types.h>
#include <input.h>
#include <render_game.h>

/**
 * This file represents platform logic that is exported to the game.
 */

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

    // Platform-owned, outside the bytewise game-memory snapshot. The vectors
    // own heap memory and must be checkpointed using normal deep copies.
    // Players are added through AddInputPlayer, which keeps these the same size.
    InputSpecification* inputSpecification;
    std::vector<PlayerData> playerData;
    // Input merged across frames until consumed by fixed timestep updates.
    std::vector<PlayerData> accumulatedPlayerData;

    // TODO(marvin): Does the imgui context really belong to game memory? Should it be part of the debug storage?
    ImGuiContext *imGuiContext;

#if SKL_INTERNAL
    void* debugStorage;
    DebugState* debugState;
#endif

    PlatformAPI platformAPI;

    // NOTE(marvin): It is contained in the scene as well, but the SKL physics system's temp allocator need to be reset due to vtable x hot reload conflict. Putting it here for convenience.
    // TODO: It would be ideal to prevent hot reloading logic of sklPhysicsSystem to spread into game code
    //       however we will hold off on abstracting dylib logic until another library like Jolt necessitates the same kind of behavior.
    void* sklPhysicsSystem;
};


// NOTE(marvin): Load is for setting up infrastructure, anything that
// has to be done after the game module is loaded in, including after
// hot reloads, whereas initialize is anything has to be done at the
// start ONCE. Thus, load happens before game initialize on game boot,
// and also before game update and render on hot reload (obviously).

#define GAME_LOAD(name) void name(GameMemory &memory, b32 editor, b32 gameInitialized)
typedef GAME_LOAD(game_load_t);

// TODO(marvin): Need a different way around this with monolithic build. How is Jolt linked for monolithic build?
// Produces a list of c-style strings, or nullptr if none.
#define GAME_GET_PERSISTENT_DLL_PATHS(name) void name(const char** pathBuffer)
typedef GAME_GET_PERSISTENT_DLL_PATHS(game_get_persistent_dll_paths_t);

#define GAME_INITIALIZE(name) void name(GameMemory &memory, std::string mapName, b32 editor)
typedef GAME_INITIALIZE(game_initialize_t);

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory &memory, f32 frameTime)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);

#if SKL_STATIC_MONOLITHIC
extern "C"
{
    GAME_LOAD(GameLoad);
    GAME_INITIALIZE(GameInitialize);
    GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
}
#endif
