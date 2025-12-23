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

EntityID currentCamera = -1;

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

    s32 rv = LoadScene(scene, "test");
    if (rv != 0)
    {
        std::cout << "Failed to load scene\n";
        exit(-1);
    }

    bool slowStep = false;

    if (editor)
    {
        currentCamera = scene.NewEntity();
        CameraComponent* camera = scene.Assign<CameraComponent>(currentCamera);
        FlyingMovement* movement = scene.Assign<FlyingMovement>(currentCamera);
        scene.Assign<Transform3D>(currentCamera);

        EditorSystem *editorSystem = new EditorSystem(currentCamera);
        scene.AddSystem(editorSystem);
    }
    else
    {
        SKLPhysicsSystem *physicsSys = new SKLPhysicsSystem();
        scene.AddSystem(physicsSys);

        MovementSystem *movementSys = new MovementSystem();
        BuilderSystem *builderSys = new BuilderSystem(slowStep);
        scene.AddSystem(movementSys);
        scene.AddSystem(builderSys);

        // Get the main camera view
        SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(scene);
        if (cameraView.begin() != cameraView.end())
        {
            currentCamera = *cameraView.begin();
        }
    }

    scene.InitSystems();
}

void UpdateRenderer(Scene& scene)
{
    if (currentCamera == -1)
    {
        return;
    }

    CameraComponent *camera = scene.Get<CameraComponent>(currentCamera);
    Transform3D *cameraTransform = scene.Get<Transform3D>(currentCamera);

    std::vector<DirLightRenderInfo> dirLights;
    for (EntityID ent: SceneView<DirLight, Transform3D>(scene))
    {
        DirLight *l = scene.Get<DirLight>(ent);
        if (l->lightID == -1)
        {
            l->lightID = globalPlatformAPI.rendererAddDirLight();
        }

        Transform3D *lTransform = scene.Get<Transform3D>(ent);

        dirLights.push_back({l->lightID, lTransform, l->diffuse, l->specular});
    }

    std::vector<SpotLightRenderInfo> spotLights;
    for (EntityID ent: SceneView<SpotLight, Transform3D>(scene))
    {
        SpotLight *l = scene.Get<SpotLight>(ent);
        if (l->lightID == -1)
        {
            l->lightID = globalPlatformAPI.rendererAddSpotLight();
        }

        Transform3D *lTransform = scene.Get<Transform3D>(ent);

        spotLights.push_back({l->lightID, lTransform, l->diffuse, l->specular,
                                 l->innerCone, l->outerCone, l->range, true});
    }

    std::vector<PointLightRenderInfo> pointLights;
    for (EntityID ent: SceneView<PointLight, Transform3D>(scene))
    {
        PointLight *l = scene.Get<PointLight>(ent);
        if (l->lightID == -1)
        {
            l->lightID = globalPlatformAPI.rendererAddPointLight();
        }

        Transform3D *lTransform = scene.Get<Transform3D>(ent);

        pointLights.push_back({l->lightID, lTransform, l->diffuse, l->specular,
                                  l->constant, l->linear, l->quadratic, l->maxRange, true});
    }

    std::vector<MeshRenderInfo> meshInstances;
    for (EntityID ent: SceneView<MeshComponent, Transform3D>(scene))
    {
        Transform3D *t = scene.Get<Transform3D>(ent);
        glm::mat4 model = t->GetWorldTransform();
        MeshComponent *m = scene.Get<MeshComponent>(ent);
        MeshID meshID = m->mesh == nullptr ? -1 : m->mesh->id;
        TextureID texID = m->texture == nullptr ? -1 : m->texture->id;
        meshInstances.push_back({model, m->color, meshID, texID, GetEntityIndex(ent)});
    }

    RenderFrameInfo sendState{
        .cameraTransform = cameraTransform,
        .meshes = meshInstances,
        .dirLights = dirLights,
        .spotLights = spotLights,
        .pointLights = pointLights,
        .cameraFov = camera->fov,
        .cameraNear = camera->nearPlane,
        .cameraFar = camera->farPlane
    };

    globalPlatformAPI.rendererRenderUpdate(sendState);
}


extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    scene.UpdateSystems(&input, deltaTime);

    UpdateRenderer(scene);

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
