#include "game.h"

#include <map>
#include <random>
// TODO(marvin): Interactions with threads should probably go through platform API.
#include <thread>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "asset_types.h"

#include "ecs.cpp"

#include "math/skl_math_utils.h"


global_variable PlatformAPI globalPlatformAPI;

#include "scene_loader.cpp"
#include "components.cpp"

#include "physics.cpp"
#include "systems.cpp"

local void LogDebugRecords();

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_INITIALIZE(GameInitialize)
{
    RegisterComponents(scene);

    globalPlatformAPI = platformAPI;

    RenderPipelineInitInfo initDesc {};
    globalPlatformAPI.rendererInitPipelines(initDesc);

    LoadScene(scene, "scenes/city.toml");

    bool slowStep = false;

    SKLPhysicsSystem *characterControllerSys = new SKLPhysicsSystem();
    scene.AddSystem(characterControllerSys);

    RenderSystem *renderSys = new RenderSystem();
    MovementSystem *movementSys = new MovementSystem();
    BuilderSystem *builderSys = new BuilderSystem(slowStep);
    scene.AddSystem(renderSys);
    scene.AddSystem(movementSys);
    scene.AddSystem(builderSys);
    scene.InitSystems();
}

// NOTE(marvin): Our logger doesn't have string format...
// Using c std lib's one for now.
#if SKL_INTERNAL
#include <cstdio>
#endif


extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    scene.UpdateSystems(&input, deltaTime);
    LogDebugRecords();
}

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
