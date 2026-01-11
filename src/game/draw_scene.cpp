#include <vector>

#include <draw_scene.h>
#include <meta_definitions.h>
#include <game.h>
#include <components.h>
#include <scene.h>
#include <scene_loader.h>
#include <scene_view.h>

void FindCamera(GameState &gameState)
{
    // Get the main camera view
    SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(gameState.scene);
    if (cameraView.begin() != cameraView.end())
    {
        gameState.currentCamera = *cameraView.begin();
    }
}

void DrawScene(GameState &gameState, GameInput &input, f32 deltaTime)
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