#include <imgui.h>

std::vector<glm::vec4> getFrustumCorners(const glm::mat4& proj, const glm::mat4& view)
{
    glm::mat4 inverse = glm::inverse(proj * view);

    std::vector<glm::vec4> frustumCorners;
    for (u32 x = 0; x < 2; ++x)
    {
        for (u32 y = 0; y < 2; ++y)
        {
            for (u32 z = 0; z < 2; ++z)
            {
                const glm::vec4 pt =
                        inverse * glm::vec4(
                                    2.0f * x - 1.0f,
                                    2.0f * y - 1.0f,
                                    z,
                                    1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}

#define NUM_CASCADES 6

class RenderSystem : public System
{
    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        // Get the main camera view
        SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(*scene);
        if (cameraView.begin() == cameraView.end())
        {
            return;
        }

        EntityID cameraEnt = *cameraView.begin();
        CameraComponent *camera = scene->Get<CameraComponent>(cameraEnt);
        Transform3D *cameraTransform = scene->Get<Transform3D>(cameraEnt);

        std::vector<DirLightRenderInfo> dirLights;
        for (EntityID ent: SceneView<DirLight, Transform3D>(*scene))
        {
            DirLight *l = scene->Get<DirLight>(ent);
            if (l->lightID == -1)
            {
                l->lightID = AddDirLight();
            }

            Transform3D *lTransform = scene->Get<Transform3D>(ent);

            dirLights.push_back({l->lightID, lTransform, l->diffuse, l->specular});
        }

        std::vector<SpotLightRenderInfo> spotLights;
        for (EntityID ent: SceneView<SpotLight, Transform3D>(*scene))
        {
            SpotLight *l = scene->Get<SpotLight>(ent);
            if (l->lightID == -1)
            {
                l->lightID = AddSpotLight();
            }

            Transform3D *lTransform = scene->Get<Transform3D>(ent);

            spotLights.push_back({l->lightID, lTransform, l->diffuse, l->specular,
                                  l->innerCone, l->outerCone, l->range, true});
        }

        std::vector<PointLightRenderInfo> pointLights;
        for (EntityID ent: SceneView<PointLight, Transform3D>(*scene))
        {
            PointLight *l = scene->Get<PointLight>(ent);
            if (l->lightID == -1)
            {
                l->lightID = AddPointLight();
            }

            Transform3D *lTransform = scene->Get<Transform3D>(ent);

            pointLights.push_back({l->lightID, lTransform, l->diffuse, l->specular,
                                   l->constant, l->linear, l->quadratic, l->maxRange, true});
        }

        std::vector<MeshRenderInfo> meshInstances;
        for (EntityID ent: SceneView<MeshComponent, Transform3D>(*scene))
        {
            Transform3D *t = scene->Get<Transform3D>(ent);
            glm::mat4 model = t->GetWorldTransform();
            MeshComponent *m = scene->Get<MeshComponent>(ent);
            MeshID meshID = m->mesh == nullptr ? -1 : m->mesh->id;
            TextureID texID = m->texture == nullptr ? -1 : m->texture->id;
            meshInstances.push_back({model, m->color, meshID, texID});
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

        RenderUpdate(sendState);
    }
};

// TODO(marvin): Figure out a better place to put this, for it is not
// a system, and too generalisable for it to be a private method on
// CharacterControllerSystem.

local JPH::Vec3 GetMovementDirectionFromInput(GameInput *input)
{
    // NOTE(marvin): Jolt uses right-hand coordinate system with Y up.
    JPH::Vec3 result = {};
    if (input->keysDown.contains("W"))
    {
        result = JPH::Vec3(0, 0, 1);
    }
    else if (input->keysDown.contains("S"))
    {
        result = JPH::Vec3(0, 0, -1);
    }
    else if (input->keysDown.contains("D"))
    {
        result = JPH::Vec3(-1, 0, 0);
    }
    else if (input->keysDown.contains("A"))
    {
        result = JPH::Vec3(1, 0, 0);
    }
    return result;
}

class CharacterControllerSystem : public System
{
private:
    JPH::PhysicsSystem *physicsSystem;
    JPH::TempAllocatorImpl *allocator;

    void MoveCharacterVirtual(JPH::CharacterVirtual &characterVirtual, JPH::PhysicsSystem &physicsSystem,
                              JPH::Vec3 movementDirection, f32 deltaTime)
    {
        characterVirtual.SetLinearVelocity(movementDirection);

        JPH::Vec3Arg gravity = JPH::Vec3(0, -9.81f, 0);
        JPH::CharacterVirtual::ExtendedUpdateSettings settings;
        // NOTE(marvin): I threw in a random number that seems reasonably big... I don't actually know
        // how much memory ExtendedUpdate needs...
        characterVirtual.ExtendedUpdate(deltaTime,
                                        gravity,
                                        settings,
                                        physicsSystem.GetDefaultBroadPhaseLayerFilter(Layer::MOVING),
                                        physicsSystem.GetDefaultLayerFilter(Layer::MOVING),
                                        {},
                                        {},
                                        *allocator);
    }

public:
    CharacterControllerSystem(JPH::PhysicsSystem *ps)
    {
        physicsSystem = ps;
        allocator = new JPH::TempAllocatorImpl(1024*1024*16);
    }

    void OnStart(Scene *scene)
    {

    }

    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        SceneView<PlayerCharacter, Transform3D> playerView = SceneView<PlayerCharacter, Transform3D>(*scene);
        if (playerView.begin() == playerView.end())
        {
            return;
        }

        SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(*scene);
        if (playerView.begin() == playerView.end())
        {
            return;
        }
        EntityID playerEnt = *playerView.begin();
        PlayerCharacter *pc = scene->Get<PlayerCharacter>(playerEnt);
        JPH::CharacterVirtual *cv = pc->characterVirtual;
        Transform3D *pt = scene->Get<Transform3D>(playerEnt);

        EntityID cameraEnt = *playerView.begin();
        Transform3D *ct = scene->Get<Transform3D>(cameraEnt);

        glm::vec3 ip = pt->GetLocalPosition();
        JPH::Vec3 playerPhysicsInitialPosition = JPH::Vec3(-ip.y, ip.z, ip.x);
        cv->SetPosition(playerPhysicsInitialPosition);

        glm::vec3 ir = pt->GetLocalRotation();
        JPH::Quat playerPhysicsInitialRotation = JPH::Quat(-ir.y, ir.z, ir.x, 1.0f).Normalized();
        cv->SetRotation(playerPhysicsInitialRotation);

        JPH::Vec3 movementDirection = GetMovementDirectionFromInput(input);
        MoveCharacterVirtual(*cv, *physicsSystem, movementDirection, deltaTime);

        // Update player and camera transforms from character virtual's position
        JPH::Vec3 cp = cv->GetPosition();
        pt->GetLocalPosition() = glm::vec3(cp.GetZ(), -cp.GetX(), cp.GetY());
#if 0
        ct->position = glm::vec3(cp.GetZ(), -cp.GetX(), cp.GetY());
#endif
    }
};

class MovementSystem : public System
{
    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        for (EntityID ent: SceneView<FlyingMovement, Transform3D>(*scene))
        {
            FlyingMovement *f = scene->Get<FlyingMovement>(ent);
            Transform3D *t = scene->Get<Transform3D>(ent);

            t->AddLocalRotation({0, 0, input->mouseDeltaX * f->turnSpeed});
            t->AddLocalRotation({0, input->mouseDeltaY * f->turnSpeed, 0});
            t->SetLocalRotation({t->GetLocalRotation().x, std::min(std::max(t->GetLocalRotation().y, -90.0f), 90.0f), t->GetLocalRotation().z});

            if (input->keysDown.contains("W"))
            {
                t->AddLocalPosition(t->GetForwardVector() * f->moveSpeed * deltaTime);
            }

            if (input->keysDown.contains("S"))
            {
                t->AddLocalPosition(t->GetForwardVector() * -f->moveSpeed * deltaTime);
            }

            if (input->keysDown.contains("D"))
            {
                t->AddLocalPosition(t->GetRightVector() * f->moveSpeed * deltaTime);
            }

            if (input->keysDown.contains("A"))
            {
                t->AddLocalPosition(t->GetRightVector() * -f->moveSpeed * deltaTime);
            }
        }
    }
};


// A vocabulary
//
// - EC: Road systems with extra credit
//
// A set of production rules
//
// - Subdivide: 5 -> 55
// - Plane Extend: 5 -> 5
//   - Rotate (inscribe)
//   - Place cuboid                - DONE
//   - Place trapezoid             - DONE
// - Pyramid Roof: 5 -> (6 | T)
// - Prism Roof: 5 -> T
// - Antenna: 6 -> T               - DONE
//
// An “axiom” (i.e. start state)
// 5
// A flat 2D plane that spans our entire city.

class BuilderSystem : public System
{
private:
    bool slowStep = false;
    f32 timer = 2.0f; // Seconds until next step
    f32 rate = 0.5f;   // Steps per second

    u32 pointLightCount = 0;
public:
    BuilderSystem(bool slowStep)
    {
        this->slowStep = slowStep;
    }

    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        if (slowStep && timer > 0.0f)
        {
            timer -= deltaTime;
        }
        else
        {
            timer = 1.0f / rate;
            Step(scene);
        }
    }

    constexpr static f32 antennaHeightMin = 128;
    constexpr static f32 antennaHeightMax = 192;
    constexpr static f32 antennaWidth = 8;

    constexpr static f32 cuboidHeightMin = 32;
    constexpr static f32 cuboidHeightMax = 96;

    constexpr static f32 trapHeightMin = 24;
    constexpr static f32 trapHeightMax = 48;

    constexpr static f32 roofHeightMin = 32;
    constexpr static f32 roofHeightMax = 64;

    void Step(Scene *scene)
    {
        // Plane Rules
        for (EntityID ent: SceneView<Plane, Transform3D>(*scene))
        {
            Transform3D *t = scene->Get<Transform3D>(ent);
            Plane *plane = scene->Get<Plane>(ent);

            if (plane->width <= 16.0f || plane->length <= 16.0f || (plane->width / plane->length) >= 128 || (plane->length / plane->width) >= 128)
            {
                if (RandInBetween(0.0f, 1.0f) > 0.975f)
                {
                    // Build antenna
                    f32 antennaHeight = RandInBetween(antennaHeightMin, antennaHeightMax);
                    BuildPart(scene, ent, t, globalPlatformAPI.platformLoadMeshAsset("cube"), {antennaWidth, antennaWidth, antennaHeight});
                    t->AddLocalPosition({0, 0, -antennaWidth / 2});

                    if (pointLightCount < 64)
                    {
                        EntityID pointLight = scene->NewEntity();
                        Transform3D* pointTransform = scene->Assign<Transform3D>(pointLight);
                        *pointTransform = *t;
                        pointTransform->AddLocalPosition({0, 0, antennaHeight / 2});
                        pointTransform->SetLocalScale({16, 16, 16});
                        PointLight* pointLightComponent = scene->Assign<PointLight>(pointLight);
                        f32 red = RandInBetween(0.8, 1.0);
                        pointLightComponent->diffuse = {red, 0.6, 0.25};
                        pointLightComponent->specular = {red, 0.6, 0.25};
                        pointLightComponent->constant = 1;
                        pointLightComponent->linear = 0.0005;
                        pointLightComponent->quadratic = 0.00005;
                        pointLightComponent->maxRange = 1000;

                        pointLightCount++;
                    }
                }

                scene->Remove<Plane>(ent);
                continue;
            }

            switch (RandInt(0, 13))
            {
            case 0:
                {
                    // Rotate
                    f32 shortSide = std::min(plane->width, plane->length);
                    f32 longSide = std::max(plane->width, plane->length);

                    f32 maxAngle = atan2(shortSide, longSide) - 0.02f;

                    f32 angle = RandInBetween(glm::radians(7.5f), maxAngle);

                    f32 costheta = cos(angle);
                    f32 sintheta = sin(angle);
                    f32 denom = ((costheta * costheta) - (sintheta * sintheta));
                    f32 width = ((plane->width * costheta) -
                                 (plane->length * sintheta)) / denom;
                    f32 length = ((plane->length * costheta) -
                                  (plane->width * sintheta)) / denom;
                    plane->width = width;
                    plane->length = length;

                    t->AddLocalRotation({0, 0, glm::degrees(angle)});
                    break;
                }
            case 1:
                {
                    // Build Trapezoid
                    if (plane->width > 256 || plane->length > 256)
                    {
                        continue;
                    }

                    f32 trapHeight = RandInBetween(trapHeightMin, trapHeightMax);
                    BuildPart(scene, ent, t, globalPlatformAPI.platformLoadMeshAsset("trap"), {plane->length, plane->width, trapHeight});

                    EntityID newPlane = scene->NewEntity();
                    Transform3D *newT = scene->Assign<Transform3D>(newPlane);
                    Plane *p = scene->Assign<Plane>(newPlane);
                    *newT = *t;
                    newT->AddLocalPosition({0, 0, trapHeight / 2});
                    p->width = plane->width / 2;
                    p->length = plane->length / 2;

                    scene->Remove<Plane>(ent);
                    break;
                }
            case 2:
                {
                    // Build Pyramid Roof
                    if (plane->width > 96 || plane->length > 96)
                    {
                        continue;
                    }

                    f32 pyraHeight = RandInBetween(roofHeightMin, roofHeightMax);
                    BuildPart(scene, ent, t, globalPlatformAPI.platformLoadMeshAsset("pyra"), {plane->length, plane->width, pyraHeight});

                    scene->Remove<Plane>(ent);
                    break;
                }
            case 3:
                {
                    // Build Prism Roof
                    if (plane->width > 96)
                    {
                        continue;
                    }

                    f32 prismHeight = RandInBetween(roofHeightMin, roofHeightMax);
                    BuildPart(scene, ent, t, globalPlatformAPI.platformLoadMeshAsset("prism"), {plane->length, plane->width, prismHeight});

                    scene->Remove<Plane>(ent);
                    break;
                }
            case 4:
            case 5:
            case 6:
            case 7:
                {
                    // Build Cuboid
                    f32 cuboidHeight = RandInBetween(cuboidHeightMin, cuboidHeightMax);
                    BuildPart(scene, ent, t, globalPlatformAPI.platformLoadMeshAsset("cube"), {plane->length, plane->width, cuboidHeight});

                    EntityID newPlane = scene->NewEntity();
                    Transform3D *newT = scene->Assign<Transform3D>(newPlane);
                    Plane *p = scene->Assign<Plane>(newPlane);
                    *newT = *t;
                    newT->AddLocalPosition({0, 0, cuboidHeight / 2});
                    *p = *plane;

                    scene->Remove<Plane>(ent);
                    break;
                }
            default:
                {
                    // Subdivide
                    EntityID newPlane = scene->NewEntity();
                    Transform3D *newT = scene->Assign<Transform3D>(newPlane);
                    Plane *p = scene->Assign<Plane>(newPlane);
                    *newT = *t;
                    *p = *plane;

                    f32 ratio = RandInBetween(0.2f, 0.8f);

                    if (RandInBetween(0.0f, plane->width + plane->length) < plane->length)
                    {
                        // Split X axis
                        f32 old = plane->length;
                        f32 divisible = plane->length - 16.0f;

                        plane->length = divisible * ratio;
                        p->length = divisible * (1.0f - ratio);

                        t->AddLocalPosition(glm::normalize(t->GetForwardVector()) * ((old - plane->length) * -0.5f));
                        newT->AddLocalPosition(glm::normalize(newT->GetForwardVector()) * ((old - p->length) * 0.5f));
                    }
                    else
                    {
                        // Split Y axis
                        f32 old = plane->width;
                        f32 divisible = plane->width - 16.0f;

                        plane->width = divisible * ratio;
                        p->width = divisible * (1.0f - ratio);

                        t->AddLocalPosition(glm::normalize(t->GetRightVector()) * ((old - plane->width) * -0.5f));
                        newT->AddLocalPosition(glm::normalize(newT->GetRightVector()) * ((old - p->width) * 0.5f));
                    }
                }
            }
        }
    }

    void BuildPart(Scene *scene, EntityID ent, Transform3D *t, MeshAsset *mesh, glm::vec3 scale)
    {
        t->AddLocalPosition({0, 0, scale.z / 2});
        t->SetLocalScale(scale);

        MeshComponent *m = scene->Assign<MeshComponent>(ent);
        m->mesh = mesh;
        f32 shade = RandInBetween(0.25f, 0.75f);
        m->color = {shade, shade, shade};
    }
};

class EditorSystem : public System
{
    EntityID editorCam;

public:
    EditorSystem(EntityID editorCam)
    {
        this->editorCam = editorCam;
    }

    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        if (input->keysDown.contains("Mouse 3"))
        {
            FlyingMovement *f = scene->Get<FlyingMovement>(editorCam);
            Transform3D *t = scene->Get<Transform3D>(editorCam);

            t->AddLocalRotation({0, 0, input->mouseDeltaX * f->turnSpeed});
            t->AddLocalRotation({0, input->mouseDeltaY * f->turnSpeed, 0});
            t->SetLocalRotation({t->GetLocalRotation().x, std::min(std::max(t->GetLocalRotation().y, -90.0f), 90.0f), t->GetLocalRotation().z});

            if (input->keysDown.contains("W"))
            {
                t->AddLocalPosition(t->GetForwardVector() * f->moveSpeed * deltaTime);
            }

            if (input->keysDown.contains("S"))
            {
                t->AddLocalPosition(t->GetForwardVector() * -f->moveSpeed * deltaTime);
            }

            if (input->keysDown.contains("D"))
            {
                t->AddLocalPosition(t->GetRightVector() * f->moveSpeed * deltaTime);
            }

            if (input->keysDown.contains("A"))
            {
                t->AddLocalPosition(t->GetRightVector() * -f->moveSpeed * deltaTime);
            }
        }
    }
};
