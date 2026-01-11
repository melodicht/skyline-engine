#include <array>

#include <imgui.h>

#include <meta_definitions.h>
#include <scene.h>
#include <game.h>
#include <scene_loader.h>
#include <components.h>
#include <physics.h>
#include <overlay.h>
#include <city_builder.h>
#include <editor.h>
#include <movement.h>


struct GameState
{
    Scene scene;

    EntityID currentCamera = -1;
    b32 isEditor;

    // TODO(marvin): Overlay mode is a shared between ecs editor and debug mode. Ideally in a different struct or compiled away for the actual game release. However, because ecs editor is part of game release, cannot be compiled away.
    // NOTE(marvin): In actual release, overlay mode should only be none, and is never checked.
    OverlayMode overlayMode;
};

global_variable PlatformAPI globalPlatformAPI;

local void LogDebugRecords();

// TODO(marvin): Move these to a different file?
#if SKL_INTERNAL

local void RenderSizesViewerAllocations(DebugAllocations *allocations);

local void RenderSizesViewerRowArena(DebugArena *arena, const char *debugID, const char *name)
{
    ImGui::TableNextColumn();
    std::string memoryString = std::format("{} / {}", arena->used, arena->totalSize);
    b32 open = ImGui::TreeNode(memoryString.c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%.1f%%", static_cast<f32>(arena->used) / static_cast<f32>(arena->totalSize));
    ImGui::TableNextColumn();
    // TODO(marvin): The DebugArena struct should accumulate this information as data.
    ImGui::Text("WIP");
    ImGui::TableNextColumn();
    ImGui::Text("%s", name);
    ImGui::TableNextColumn();
    ImGui::Text("%s", debugID);

    if (open)
    {
        RenderSizesViewerAllocations(&arena->allocations);
        ImGui::TreePop();
    }
}

local void RenderSizesViewerRowRegular(DebugRegularAllocation *regular, const char *debugID)
{
    ImGui::TableNextColumn();
    ImGui::Text("%u + %u", regular->offset, regular->size);
    ImGui::TableNextColumn();
    ImGui::TextDisabled("--");
    ImGui::TableNextColumn();
    ImGui::TextDisabled("--");
    ImGui::TableNextColumn();
    ImGui::TextDisabled("--");
    ImGui::TableNextColumn();
    ImGui::Text("%s", debugID);
}

local void RenderSizesViewerRowAllocation(DebugGeneralAllocation *allocation)
{
    if (allocation->type == allocationType_arena)
    {
        RenderSizesViewerRowArena(&allocation->arena, allocation->debugID, allocation->name);
    }
    else if (allocation->type == allocationType_regular)
    {
        RenderSizesViewerRowRegular(&allocation->regular, allocation->debugID);
    }
}

local void RenderSizesViewerAllocations(DebugAllocations *allocations)
{
    for (DebugGeneralAllocation *allocation = allocations->first;
         allocation;
         allocation = allocation->next)
    {
        ImGui::TableNextRow();

        RenderSizesViewerRowAllocation(allocation);
    }
}

#endif

// NOTE(marvin): ECS editor functionality in the editor system.
local void RenderOverlay(GameState &gameState)
{
    b32 shouldShowOverlay = gameState.isEditor;

#if SKL_INTERNAL
    shouldShowOverlay |= true;
#endif

    if (shouldShowOverlay)
    {
        ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize({viewport->Size.x, 0});

        ImGui::Begin("Overlay", nullptr, window_flags);
        
        // NOTE(marvin): Tabs
        ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("Overlay Options", tabBarFlags))
        {
            if (gameState.isEditor && ImGui::BeginTabItem("ECS Editor"))
            {
                gameState.overlayMode = overlayMode_ecsEditor;
                ImGui::EndTabItem();
            }
#if SKL_INTERNAL
            if (ImGui::BeginTabItem("Memory"))
            {
                gameState.overlayMode = overlayMode_memory;
                ImGui::EndTabItem();
            }
#endif
            ImGui::EndTabBar();
        }


#if SKL_INTERNAL
        // NOTE(marvin): Tab content.
        if (gameState.overlayMode == overlayMode_memory)
        {
            DebugState *debugState = globalDebugState;
            
            if (ImGui::BeginTabBar("Memory Options", tabBarFlags))
            {
                if (ImGui::BeginTabItem("Sizes Viewer"))
                {
                    ImGuiTableFlags tableFlags =
                    ImGuiTableFlags_BordersV |
                    ImGuiTableFlags_BordersOuterH |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_NoBordersInBody;

                    if (ImGui::BeginTable("Sizes Viewer Table", 5, tableFlags))
                    {
                        // NOTE(marvin): Headers
                        ImGui::TableSetupColumn("MEMORY");
                        ImGui::TableSetupColumn("PERCENT");
                        ImGui::TableSetupColumn("ALLOCATIONS");
                        ImGui::TableSetupColumn("NAME");
                        ImGui::TableSetupColumn("DEBUG ID");
                        ImGui::TableHeadersRow();

                        RenderSizesViewerAllocations(&debugState->targets);

                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
#endif

        ImGui::End();
    }
}

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_INITIALIZE(GameInitialize)
{
    DebugInitialize(memory);
    
    Assert(sizeof(GameState) <= memory.permanentStorageSize);
    GameState *gameState = static_cast<GameState *>(memory.permanentStorage);

    // NOTE(marvin): The remaining arena here only exists for the
    // duration of game initialize. The scene has its own set of
    // arenas, which is takes from the remaining arena here. Once the
    // scene is fully initialized, the remaining arena has done its
    // job and is no longer needed. Recall that the memory arena is
    // just a wrapper around a pointer to some memory storage, with
    // some book-keeping information about how that memory storage is
    // used. The book-keeping of remaining arena here is for the sole
    // purpose of starting up the scene's own memory arenas.
    u8 *pastGameStateAddress = static_cast<u8 *>(memory.permanentStorage) + sizeof(GameState);
    MemoryArena remainingArena = InitMemoryArena(pastGameStateAddress, memory.permanentStorageSize - sizeof(GameState), "GameArena");

    gameState->overlayMode = overlayMode_none;
    gameState->scene = Scene(&remainingArena);
    Scene &scene = gameState->scene;

    gameState->isEditor = editor;
    globalPlatformAPI = memory.platformAPI;

    RenderPipelineInitInfo initDesc {};
    globalPlatformAPI.renderer.InitPipelines(initDesc);

    globalPlatformAPI.assetUtils.LoadSkyboxAsset({"YokohamaSkybox/posx", "YokohamaSkybox/negx", "YokohamaSkybox/posy", "YokohamaSkybox/negy", "YokohamaSkybox/posz", "YokohamaSkybox/negz"});

    RegisterComponents(scene, editor);

    s32 rv = LoadScene(scene, mapName);
    if (rv != 0)
    {
        std::cout << "Failed to load scene\n";
        exit(-1);
    }

    b32 slowStep = false;

    if (editor)
    {
        gameState->overlayMode = overlayMode_ecsEditor;
        gameState->currentCamera = scene.NewEntity();
        CameraComponent* camera = scene.Assign<CameraComponent>(gameState->currentCamera);
        FlyingMovement* movement = scene.Assign<FlyingMovement>(gameState->currentCamera);
        scene.Assign<Transform3D>(gameState->currentCamera);

        EditorSystem *editorSystem = RegisterSystem(&scene, EditorSystem, gameState->currentCamera, &gameState->overlayMode);
    }
    else
    {
        RegisterSystem(&scene, SKLPhysicsSystem);
        RegisterSystem(&scene, MovementSystem);
        RegisterSystem(&scene, BuilderSystem, slowStep);

        // Get the main camera view
        SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(scene);
        if (cameraView.begin() != cameraView.end())
        {
            gameState->currentCamera = *cameraView.begin();
        }
    }

    scene.InitSystems();
}

void UpdateRenderer(GameState &gameState, GameInput &input, f32 deltaTime)
{
    if (gameState.currentCamera == -1)
    {
        return;
    }

    Scene &scene = gameState.scene;
    CameraComponent *camera = scene.Get<CameraComponent>(gameState.currentCamera);
    Transform3D *cameraTransform = scene.Get<Transform3D>(gameState.currentCamera);

    std::vector<DirLightRenderInfo> dirLights;
    for (EntityID ent: SceneView<DirLight, Transform3D>(scene))
    {
        DirLight *l = scene.Get<DirLight>(ent);
        if (l->lightID == -1)
        {
            l->lightID = globalPlatformAPI.renderer.AddDirLight();
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
            l->lightID = globalPlatformAPI.renderer.AddSpotLight();
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
            l->lightID = globalPlatformAPI.renderer.AddPointLight();
        }

        Transform3D *lTransform = scene.Get<Transform3D>(ent);

        pointLights.push_back({l->lightID, lTransform, l->diffuse, l->specular,
                                  l->radius, l->falloff, true,
                                  // Temporary for webgpu
                                  1, 0.0005, 0.00005, 1000});
    }

    std::vector<IconRenderInfo> icons;
    if (gameState.isEditor)
    {
        for (EntityID ent : SceneView<Transform3D, NameComponent>(scene))
        {
            Transform3D *iconTransform = scene.Get<Transform3D>(ent);
            for (IconGizmo& gizmo : iconGizmos)
            {
                if (scene.Has(ent, gizmo.id))
                {
                    icons.push_back({iconTransform->GetWorldPosition(), gizmo.texture->id, GetEntityIndex(ent)});
                }
            }
        }
    }

    std::vector<MeshRenderInfo> meshInstances;
    for (EntityID ent: SceneView<MeshComponent, Transform3D>(scene))
    {
        Transform3D *t = scene.Get<Transform3D>(ent);
        glm::mat4 model = t->GetWorldTransform();
        MeshComponent *m = scene.Get<MeshComponent>(ent);
        if (m->mesh != nullptr)
        {
            MeshID meshID = m->mesh->id;
            TextureID texID = m->texture == nullptr ? -1 : m->texture->id;
            meshInstances.push_back({model, m->color, meshID, texID, GetEntityIndex(ent)});
        }
    }

    RenderFrameInfo sendState{
        .cameraTransform = cameraTransform,
        .meshes = meshInstances,
        .dirLights = dirLights,
        .spotLights = spotLights,
        .pointLights = pointLights,
        .cameraFov = camera->fov,
        .cameraNear = camera->nearPlane,
        .cameraFar = camera->farPlane,
        .cursorPos = {input.mouseX, input.mouseY},
        .icons = icons
    };

    globalPlatformAPI.renderer.RenderUpdate(sendState);
}

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_LOAD(GameLoad)
{
#if SKL_ENABLED_EDITOR
    ImGui::SetCurrentContext(memory.imGuiContext);
#endif

    DebugUpdate(memory);

    globalPlatformAPI = memory.platformAPI;
}

extern "C"
#if defined(_WIN32) || defined(_WIN64)
__declspec(dllexport)
#endif
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(GameState) <= memory.permanentStorageSize);
    GameState *gameState = static_cast<GameState *>(memory.permanentStorage);
    Scene &scene = gameState->scene;

    // NOTE(marvin): Putting RenderOverlay above UpdateSystems so that EditorSystem's
    // GUI overlay will go below the tabs.
    RenderOverlay(*gameState);
    scene.UpdateSystems(&input, deltaTime);

    UpdateRenderer(*gameState, input, deltaTime);

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
