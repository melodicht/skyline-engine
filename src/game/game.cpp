#include <array>

#include <imgui.h>

#if SKL_ENABLED_EDITOR
#include <editor.h>
#endif

#include <debug.h>
#include <game.h>
#include <meta_definitions.h>
#include <scene.h>
#include <map_loader.h>
#include <system_registry.h>
#include <components.h>
#include <physics.h>
#include <overlay.h>
#include <city_builder.h>

#include <movement.h>
#include <draw_scene.h>

constexpr u32 FIXED_SIZE_STORAGE_SIZE = Megabytes(512 + 256);

constexpr f32 FIXED_TIMESTEP_DELTA_TIME = 1.0f / 60.0f;

PlatformAssetUtils assetUtils;
PlatformRenderer renderer;
PlatformAllocator allocator;

#if SKL_INTERNAL
DebugState* globalDebugState;
#endif

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_INITIALIZE(GameInitialize)
{
    memory.fixedSizeStorage = allocator.AlignedAllocate(FIXED_SIZE_STORAGE_SIZE, 8);

    DebugInitialize(memory);
    
    Assert(sizeof(GameState) <= FIXED_SIZE_STORAGE_SIZE);
    GameState *gameState = static_cast<GameState *>(memory.fixedSizeStorage);

    // TODO(marvin): We currently allocate WAY more memory than we actually use... got to revisit how much memory we actually need.

    // NOTE(marvin): The remaining arena here only exists for the
    // duration of game initialize. The scene has its own set of
    // arenas, which is takes from the remaining arena here. Once the
    // scene is fully initialized, the remaining arena has done its
    // job and is no longer needed. Recall that the memory arena is
    // just a wrapper around a pointer to some memory storage, with
    // some book-keeping information about how that memory storage is
    // used. The book-keeping of remaining arena here is for the sole
    // purpose of starting up the scene's own memory arenas.
    u8 *pastGameStateAddress = static_cast<u8 *>(memory.fixedSizeStorage) + sizeof(GameState);
    MemoryArena remainingArena = InitMemoryArena(pastGameStateAddress, FIXED_SIZE_STORAGE_SIZE - sizeof(GameState), "GameArena");

    gameState->overlayMode = overlayMode_none;
    gameState->scene = Scene(&remainingArena);
    Scene &scene = gameState->scene;

    assetUtils.LoadSkyboxAsset({"YokohamaSkybox/posx", "YokohamaSkybox/negx", "YokohamaSkybox/posy", "YokohamaSkybox/negy", "YokohamaSkybox/posz", "YokohamaSkybox/negz"});

    CreateComponentPools(scene);

    s32 rv = LoadMap(scene, mapName);
    if (rv != 0)
    {
        std::cout << "Failed to load map\n";
        exit(-1);
    }

    memory.sklPhysicsSystem = {};

    b32 slowStep = false;
    #if SKL_ENABLED_EDITOR
    gameState->isEditor = editor;
    if (editor)
    {
        gameState->overlayMode = overlayMode_ecsEditor;
        gameState->currentCamera = scene.NewEntity();
        CameraComponent* camera = scene.Assign<CameraComponent>(gameState->currentCamera);
        FlyingMovement* movement = scene.Assign<FlyingMovement>(gameState->currentCamera);
        scene.Assign<Transform3D>(gameState->currentCamera);

        EditorSystem *editorSystem = scene.CreateVariableTimestepSystem<EditorSystem>(gameState->currentCamera, &gameState->overlayMode);
    }
    else
    {
    #endif
        memory.sklPhysicsSystem = static_cast<void*>(scene.CreateSemifixedTimestepSystem<SKLPhysicsSystem>());
        scene.CreateSemifixedTimestepSystem<MovementSystem>();
        scene.CreateSemifixedTimestepSystem<BuilderSystem>(slowStep);

        FindCamera(*gameState);
    #if SKL_ENABLED_EDITOR
    }
    #endif

    scene.InitSystems();
}

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_LOAD(GameLoad)
{
    ImGui::SetCurrentContext(memory.imGuiContext);

    assetUtils = memory.platformAPI.assetUtils;
    renderer = memory.platformAPI.renderer;
    allocator = memory.platformAPI.allocator;

    #if SKL_INTERNAL
    globalDebugState = memory.debugState;
    #endif

    RegisterComponents(editor);

    DebugUpdate(memory);

    SKLPhysicsSystem* sklPhysicsSystem = static_cast<SKLPhysicsSystem*>(memory.sklPhysicsSystem);
    if (sklPhysicsSystem)
    {
        sklPhysicsSystem->Initialize();
    }
}

local void LogDebugRecords();

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    DebugUpdate(memory);
    
    Assert(sizeof(GameState) <= FIXED_SIZE_STORAGE_SIZE);
    GameState *gameState = static_cast<GameState *>(memory.fixedSizeStorage);
    Scene &scene = gameState->scene;

    // TODO(marvin): Use the interpolation technique.

    // NOTE(marvin): Putting RenderOverlay above the above systems so
    // that EditorSystem's GUI overlay will go below the tabs.
    RenderOverlay(*gameState);

    f32 remainingFrameTime = frameTime;
    while (remainingFrameTime > 0.0f)
    {
        f32 deltaTime = Minimum(remainingFrameTime, FIXED_TIMESTEP_DELTA_TIME);
        scene.UpdateSemifixedTimestepSystems(&input, deltaTime);
        remainingFrameTime -= deltaTime;
    }

    scene.UpdateVariableTimestepSystems(&input, frameTime);
    
    DrawScene(*gameState, input, frameTime);

    LogDebugRecords();
}

// NOTE(marvin): Our logger doesn't have string format...
// Using c std lib's one for now.
#if SKL_INTERNAL
#include <cstdio>
#endif

// NOTE(marvin): This has to go after ALL the timed blocks in order of
// what the preprocesser sees, so that the counter here will be the
// number of all the timed blocks that it has seen.
DebugRecord debugRecordArray[__COUNTER__];

local void LogDebugRecords()
{
#if SKL_INTERNAL
    u32 debugRecordsCount = ArrayCount(debugRecordArray);
    for (u32 i = 0;
         i < debugRecordsCount;
         ++i)
    {
        DebugRecord *debugRecord = debugRecordArray + i;

        u64 hitCount_cycleCount = AtomicExchangeU64(&debugRecord->hitCount_cycleCount, 0);
        u32 hitCount = (u32)(hitCount_cycleCount >> 32);
        u32 cycleCount = (u32)(hitCount_cycleCount & 0xFFFFFFFF);

        printf("%s:%s:%u %ucy (%uh) %ucy/h\n",
               debugRecord->blockName,
               debugRecord->fileName,
               debugRecord->lineNumber,
               cycleCount,
               hitCount,
               cycleCount / hitCount);
    }

    if (debugRecordsCount > 1)
    {
        puts("");
    }
#endif
}
