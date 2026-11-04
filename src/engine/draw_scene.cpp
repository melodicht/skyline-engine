#include <vector>

#include <draw_scene.h>
#include <editor.h>
#include <meta_definitions.h>
#include <engine.h>
#include <engine_components.h>
#include <scene.h>
#include <map_loader.h>
#include <scene_view.h>

void FindCamera(GameState &gameState)
{
    // Get the main camera view
    SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(gameState.scene);
    if (cameraView.begin() != cameraView.end())
    {
        gameState.currentCamera = std::get<0>(*cameraView.begin());
    }
}

void DrawScene(GameState &gameState, std::span<PlayerData> playerData, const InputSpecification &inputSpecification, f32 deltaTime)
{
    if (gameState.currentCamera == -1)
    {
        return;
    }

    Scene &scene = gameState.scene;
    CameraComponent *camera = scene.Get<CameraComponent>(gameState.currentCamera);
    Transform3D *cameraTransform = scene.Get<Transform3D>(gameState.currentCamera);

    SceneLightingRenderInfo sceneLighting{ };
    for (auto [entityId, lighting] : SceneView<SceneLighting>(scene))
    {
        sceneLighting= {
            .ambientLighting = lighting.ambientColor,
            .pcfWorldRange = lighting.pcfWorldRange
        };
    }

    std::vector<DirLightRenderInfo> dirLights;
    for (auto [entityId, directionalLight, lightTransform] : SceneView<DirLight, Transform3D>(scene))
    {
        if (directionalLight.lightID == -1)
        {
            directionalLight.lightID = renderer.AddDirLight();
        }

        dirLights.push_back({directionalLight.lightID, &lightTransform, directionalLight.diffuse, directionalLight.specular});
    }

    std::vector<SpotLightRenderInfo> spotLights;
    for (auto [entityId, spotlight, transform] : SceneView<SpotLight, Transform3D>(scene))
    {
        if (spotlight.lightID == -1)
        {
            spotlight.lightID = renderer.AddSpotLight();
        }

        spotLights.push_back({spotlight.lightID, &transform, spotlight.diffuse, spotlight.specular,
                                 spotlight.innerCone, spotlight.outerCone, spotlight.range, spotlight.fallOff, true});
    }

    std::vector<PointLightRenderInfo> pointLights;
    for (auto [entityId, pointlight, transform] : SceneView<PointLight, Transform3D>(scene))
    {
        if (pointlight.lightID == -1)
        {
            pointlight.lightID = renderer.AddPointLight();
        }

        pointLights.push_back({
            pointlight.lightID, &transform,
            pointlight.diffuse, pointlight.specular,
            pointlight.radius, pointlight.falloff, true});
    }

    std::vector<IconRenderInfo> icons;
    if (gameState.isEditor)
    {
        for (auto [entityId, iconTransform, nameComponent] : SceneView<Transform3D, NameComponent>(scene))
        {
            for (IconGizmo& gizmo : iconGizmos)
            {
                if (scene.Has(entityId, gizmo.id))
                {
                    icons.push_back({iconTransform.GetWorldPosition(), gizmo.texture->id, GetEntityIndex(entityId)});
                }
            }
        }
    }

    std::vector<MeshRenderInfo> meshInstances;
    for (auto [entityId, meshComponent, transform] : SceneView<MeshComponent, Transform3D>(scene))
    {
        glm::mat4 model = transform.GetWorldTransform();
        if (meshComponent.mesh != nullptr)
        {
            MeshID meshID = meshComponent.mesh->id;
            TextureID texID = meshComponent.texture == nullptr ? -1 : meshComponent.texture->id;
            meshInstances.push_back({model, meshComponent.color, meshID, texID, GetEntityIndex(entityId)});
        }
    }

    // The cursor is only gathered in the editor, where it's used for picking.
    glm::ivec2 cursorPos{ 0 };
    #if SKL_ENABLED_EDITOR
    if (gameState.isEditor)
    {
        cursorPos = glm::ivec2(PlayerInput(inputSpecification, playerData[0]).Get2dInput(InputEditorCursor));
    }
    #endif

    RenderFrameInfo sendState{
        .cameraTransform = cameraTransform,
        .meshes = meshInstances,
        .dirLights = dirLights,
        .spotLights = spotLights,
        .pointLights = pointLights,
        .cameraFov = camera->fov,
        .cameraNear = camera->nearPlane,
        .cameraFar = camera->farPlane,
        .cursorPos = cursorPos,
        .icons = icons,
        .sceneLighting = sceneLighting
    };

    renderer.RenderUpdate(sendState);
}
