#include "game.h"

#include <map>
#include <random>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "renderer/render_backend.h"
#include "asset_types.h"
#include "asset_utils.cpp"

#include "ecs.cpp"

#include "math/skl_math_utils.h"

#include "scene_loader.cpp"

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

    RenderPipelineInitInfo initDesc {};
    InitPipelines(initDesc);

    LoadScene(scene, "scenes/test.toml");

    bool slowStep = false;

    // NOTE(marvin): Initialising the physics system.
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // NOTE(marvin): This is not our ECS system! Jolt happened to name it System as well. 
    JPH::PhysicsSystem *physicsSystem = new JPH::PhysicsSystem();

    // NOTE(marvin): Pulled these numbers out of my ass.
    u32 maxBodies = 1024;
    u32 numBodyMutexes = 0;  // 0 means auto-detect.
    u32 maxBodyPairs = 1024;
    u32 maxContactConstraints = 1024;
    JPH::BroadPhaseLayerInterface *sklBroadPhaseLayer = new SklBroadPhaseLayer();
    JPH::ObjectVsBroadPhaseLayerFilter *sklObjectVsBroadPhaseLayerFilter = new SklObjectVsBroadPhaseLayerFilter();
    JPH::ObjectLayerPairFilter *sklObjectLayerPairFilter = new SklObjectLayerPairFilter();
    
    physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                        *sklBroadPhaseLayer, *sklObjectVsBroadPhaseLayerFilter,
                        *sklObjectLayerPairFilter);

    CharacterControllerSystem *characterControllerSys = new CharacterControllerSystem(physicsSystem);
    scene.AddSystem(characterControllerSys);

    RenderSystem *renderSys = new RenderSystem();
    MovementSystem *movementSys = new MovementSystem();
    BuilderSystem *builderSys = new BuilderSystem(slowStep);
    scene.AddSystem(renderSys);
    scene.AddSystem(movementSys);
    scene.AddSystem(builderSys);

//    EntityID playerCharacterEnt = scene.NewEntity();
//    Transform3D *pcTransform = scene.Assign<Transform3D>(playerCharacterEnt);
//    PlayerCharacter *playerCharacter = scene.Assign<PlayerCharacter>(playerCharacterEnt);
//
//    JPH::CharacterVirtualSettings characterVirtualSettings;
//    f32 halfHeightOfCylinder = 1.0f;
//    f32 cylinderRadius = 0.3f;
//    characterVirtualSettings.mShape = new JPH::CapsuleShape(halfHeightOfCylinder, cylinderRadius);
//    characterVirtualSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cylinderRadius);
//
//    JPH::Vec3 characterPosition = JPH::Vec3(0, 10, 0);  // Just so they are not stuck in the ground.
//    JPH::Quat characterRotation = JPH::Quat(0, 0, 0, 0);
//    JPH::CharacterVirtual *characterVirtual = new JPH::CharacterVirtual(&characterVirtualSettings, characterPosition, characterRotation, physicsSystem);
//    playerCharacter->characterVirtual = characterVirtual;


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
    for (u32 i = 0;
         i < ArrayCount(debugRecordArray);
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
    puts("\n");
#endif
}
