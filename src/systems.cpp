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
                l->lightID = globalPlatformAPI.rendererAddDirLight();
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
                l->lightID = globalPlatformAPI.rendererAddSpotLight();
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
                l->lightID = globalPlatformAPI.rendererAddPointLight();
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

        globalPlatformAPI.rendererRenderUpdate(sendState);
    }
};

// TODO(marvin): Figure out a better place to put this, for it is not
// a system, and too generalisable for it to be a private method on
// SKLPhysicsSystem.

local glm::vec3 GetMovementDirection(GameInput *input, Transform3D *t)
{
    glm::vec3 result{};
    
    if (input->keysDown.contains("W"))
    {
        result += t->GetForwardVector();
    }

    if (input->keysDown.contains("S"))
    {
        result -= t->GetForwardVector();
    }

    if (input->keysDown.contains("D"))
    {
        result += t->GetRightVector();
    }

    if (input->keysDown.contains("A"))
    {
        result -= t->GetRightVector();
    }

    return result;
}

class SKLPhysicsSystem : public System
{
private:
    JPH::PhysicsSystem *physicsSystem;
    JPH::TempAllocatorImpl *allocator;
    JPH::JobSystem *jobSystem;

    void MoveCharacterVirtual(JPH::CharacterVirtual &characterVirtual, JPH::PhysicsSystem &physicsSystem,
                              JPH::Vec3 movementDirection, f32 moveSpeed, f32 deltaTime)
    {
        JPH::Vec3 velocity = characterVirtual.GetLinearVelocity();
        JPH::Vec3Arg gravity{0, -9.81f, 0};
        velocity += gravity * deltaTime;
        velocity.SetX(0.0f);
        velocity.SetZ(0.0f);
        velocity += movementDirection * moveSpeed;
        characterVirtual.SetLinearVelocity(velocity);

        JPH::CharacterVirtual::ExtendedUpdateSettings settings;
        characterVirtual.ExtendedUpdate(deltaTime,
                                        gravity,
                                        settings,
                                        physicsSystem.GetDefaultBroadPhaseLayerFilter(Layer::MOVING),
                                        physicsSystem.GetDefaultLayerFilter(Layer::MOVING),
                                        {},
                                        {},
                                        *allocator);

        // TODO(marvin): Physics System update should happen in its own system.
        u32 collisionSteps = 1;
        this->physicsSystem->Update(deltaTime, collisionSteps, this->allocator, this->jobSystem);
    }

    static void initializePlayerCharacter(PlayerCharacter *pc, JPH::PhysicsSystem *physicsSystem)
    {
        JPH::CharacterVirtualSettings characterVirtualSettings;
        f32 halfHeightOfCylinder = 1.0f;
        f32 cylinderRadius = 0.3f;
        characterVirtualSettings.mShape = new JPH::CapsuleShape(halfHeightOfCylinder, cylinderRadius);
        characterVirtualSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cylinderRadius);

        JPH::Vec3 characterPosition = JPH::Vec3(0, 10, 0);  // Just so they are not stuck in the ground.
        JPH::Quat characterRotation = JPH::Quat(0, 0, 0, 0);
        JPH::CharacterVirtual *characterVirtual = new JPH::CharacterVirtual(&characterVirtualSettings, characterPosition, characterRotation, physicsSystem);
        pc->characterVirtual = characterVirtual;
    }

public:
    SKLPhysicsSystem()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        // NOTE(marvin): Pulled these numbers out of my ass.
        const u32 maxPhysicsJobs = 2048;
        const u32 maxPhysicsBarriers = 8;
        const u32 maxBodies = 1024;
        const u32 numBodyMutexes = 0;  // 0 means auto-detect.
        const u32 maxBodyPairs = 1024;
        const u32 maxContactConstraints = 1024;
        const u32 numPhysicsThreads = std::thread::hardware_concurrency() - 1;  // Subtract main thread
    
        JPH::JobSystemThreadPool *jobSystem = new JPH::JobSystemThreadPool(maxPhysicsJobs, maxPhysicsBarriers, numPhysicsThreads);

        // NOTE(marvin): This is not our ECS system! Jolt happened to name it System as well. 
        JPH::PhysicsSystem *physicsSystem = new JPH::PhysicsSystem();

        JPH::BroadPhaseLayerInterface *sklBroadPhaseLayer = new SklBroadPhaseLayer();
        JPH::ObjectVsBroadPhaseLayerFilter *sklObjectVsBroadPhaseLayerFilter = new SklObjectVsBroadPhaseLayerFilter();
        JPH::ObjectLayerPairFilter *sklObjectLayerPairFilter = new SklObjectLayerPairFilter();
    
        physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                            *sklBroadPhaseLayer, *sklObjectVsBroadPhaseLayerFilter,
                            *sklObjectLayerPairFilter);

        this->physicsSystem = physicsSystem;
        this->jobSystem = jobSystem;
        // TODO(marvin): Is it possible for Jolt's temp allocator to take from our memory arenas (after we have them)?
        this->allocator = new JPH::TempAllocatorImpl(1024*1024*16);
    }

    ~SKLPhysicsSystem()
    {
        delete this->allocator;
        delete this->jobSystem;
        delete this->physicsSystem;
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

        if (pc->characterVirtual == nullptr)
        {
            this->initializePlayerCharacter(pc, this->physicsSystem);
        }

        JPH::BodyInterface &bodyInterface = this->physicsSystem->GetBodyInterface();
        for (EntityID ent : SceneView<StaticBox, Transform3D>(*scene))
        {
            StaticBox *sb = scene->Get<StaticBox>(ent);
            Transform3D *t = scene->Get<Transform3D>(ent);

            if (!sb->initialized)
            {
                JPH::Vec3 joltVolume = OurToJoltCoordinateSystem(sb->volume);
                JPH::Vec3 halfExtent{
                    abs(abs(joltVolume.GetX()) / 2),
                    abs(abs(joltVolume.GetY()) / 2),
                    abs(abs(joltVolume.GetZ()) / 2)
                };
                JPH::BoxShapeSettings staticBodySettings{halfExtent, 0.05f};
                JPH::ShapeSettings::ShapeResult shapeResult = staticBodySettings.Create();
                JPH::ShapeRefC shape = shapeResult.Get();

                JPH::Vec3 position = OurToJoltCoordinateSystem(t->position);
                JPH::BodyCreationSettings bodyCreationSettings{shape, position,
                                                               JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layer::NON_MOVING};
                JPH::Body *body = bodyInterface.CreateBody(bodyCreationSettings);
                bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);

                sb->initialized = true;
            }
        }

        JPH::CharacterVirtual *cv = pc->characterVirtual;
        f32 moveSpeed = pc->moveSpeed;
        Transform3D *pt = scene->Get<Transform3D>(playerEnt);

        // Load player's transform into character virtual
        glm::vec3 ip = pt->GetLocalPosition();
        JPH::Vec3 playerPhysicsInitialPosition = OurToJoltCoordinateSystem(ip);
        cv->SetPosition(playerPhysicsInitialPosition);

        glm::vec3 ir = pt->GetLocalRotation();
        JPH::Quat playerPhysicsInitialRotation = JPH::Quat(-ir.y, ir.z, ir.x, 1.0f).Normalized();
        cv->SetRotation(playerPhysicsInitialRotation);

        glm::vec3 ourMovementDirection = GetMovementDirection(input, pt);
        JPH::Vec3 joltMovementDirection = OurToJoltCoordinateSystem(ourMovementDirection);
        MoveCharacterVirtual(*cv, *physicsSystem, joltMovementDirection, moveSpeed, deltaTime);

        // Update player's transform from character virtual's position
        JPH::Vec3 joltPosition = cv->GetPosition();
        glm::vec3 position = JoltToOurCoordinateSystem(joltPosition);
        pt->SetLocalPosition(position);
    }
};

class MovementSystem : public System
{
private:
    // Cannot look up/down to the extent where it becomes looking behind.
    void CapVerticalRotationForward(Transform3D *t)
    {
        t->SetLocalRotation({t->GetLocalRotation().x, std::min(std::max(t->GetLocalRotation().y, -90.0f), 90.0f), t->GetLocalRotation().z});
    }
public:
    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        // TODO(marvin): Duplicate looking code between FlyingMovement and the XLook family of components.
        for (EntityID ent: SceneView<FlyingMovement, Transform3D>(*scene))
        {
            FlyingMovement *f = scene->Get<FlyingMovement>(ent);
            Transform3D *t = scene->Get<Transform3D>(ent);

            t->AddLocalRotation({0, 0, input->mouseDeltaX * f->turnSpeed});
            t->AddLocalRotation({0, input->mouseDeltaY * f->turnSpeed, 0});
            this->CapVerticalRotationForward(t);

            glm::vec3 movementDirection = GetMovementDirection(input, t);
            t->AddLocalPosition(movementDirection * f->moveSpeed * deltaTime);
        }

        for (EntityID ent : SceneView<HorizontalLook, Transform3D>(*scene))
        {
            HorizontalLook *hl = scene->Get<HorizontalLook>(ent);
            Transform3D *t = scene->Get<Transform3D>(ent);
            t->AddLocalRotation({0, 0, input->mouseDeltaX * hl->turnSpeed});
        }

        for (EntityID ent : SceneView<VerticalLook, Transform3D>(*scene))
        {
            VerticalLook *vl = scene->Get<VerticalLook>(ent);
            Transform3D *t = scene->Get<Transform3D>(ent);
            t->AddLocalRotation({0, input->mouseDeltaY * vl->turnSpeed, 0});
            this->CapVerticalRotationForward(t);
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
private:
    EntityID editorCam;
    EntityID selectedEntityID = 0;

    // Diplays the data entry, and indicates whether any data has been changed.
    // Only reads the data.
    // To be called inside of ImGui scope.
    bool ImguiDisplayDataEntry(DataEntry *dataEntry, Scene &scene, EntityID ent, b32 isComponent)
    {
        // TODO(marvin): Duplicate code between the non-recursive cases, the way to abstract is also not immediately obvious.
        bool changed = false;
        switch (dataEntry->type)
        {
          case INT_ENTRY:
          {
              Assert(!isComponent);
              const char *fieldName = dataEntry->name.c_str();
              ImGui::Columns(2, nullptr, false);
              ImGui::SetColumnWidth(0, 150);
        
              ImGui::Text("%s", fieldName);
              ImGui::NextColumn();
              if (ImGui::InputInt(fieldName, &(dataEntry->intVal)))
              {
                  changed = true;
              }
              ImGui::NextColumn();
              break;
          }
          case FLOAT_ENTRY:
          {
              Assert(!isComponent);
              const char *fieldName = dataEntry->name.c_str();
              ImGui::Columns(2, nullptr, false);
              ImGui::SetColumnWidth(0, 150);
        
              ImGui::Text("%s", fieldName);
              ImGui::NextColumn();
              if (ImGui::InputFloat(fieldName, &(dataEntry->floatVal)))
              {
                  changed = true;
              }
              ImGui::NextColumn();
              break;
          }
          case BOOL_ENTRY:
          {
              Assert(!isComponent);
              const char *fieldName = dataEntry->name.c_str();
              ImGui::Columns(2, nullptr, false);
              ImGui::SetColumnWidth(0, 150);
        
              ImGui::Text("%s", fieldName);
              ImGui::NextColumn();
              if (ImGui::Checkbox(fieldName, &(dataEntry->boolVal)))
              {
                  changed = true;
              }
              ImGui::NextColumn();
              break;
          }
          case VEC_ENTRY:
          {
              Assert(!isComponent);
              const char *fieldName = dataEntry->name.c_str();
              ImGui::Columns(2, nullptr, false);
              ImGui::SetColumnWidth(0, 150);
        
              ImGui::Text("%s", fieldName);
              ImGui::NextColumn();
              
              glm::vec3 vec = dataEntry->vecVal;
              f32 xyz[3] = {vec.x, vec.y, vec.z};
              if (ImGui::InputFloat3(fieldName, xyz))
              {
                  dataEntry->vecVal = {xyz[0], xyz[1], xyz[2]};
                  changed = true;
              }
              ImGui::NextColumn();
              break;
          }
          case STR_ENTRY:
          {
              Assert(!isComponent);
              const char *fieldName = dataEntry->name.c_str();
              ImGui::Columns(2, nullptr, false);
              ImGui::SetColumnWidth(0, 150);
        
              ImGui::Text("%s", fieldName);
              ImGui::NextColumn();

              // TODO(marvin): Need to have temporary string buffer for string value.
              // NOTE(marvin): Pulled that number out of my ass.
              char buf[256];
              const char *fieldStringValue = dataEntry->stringVal.c_str();
              strncpy(buf, fieldStringValue, sizeof(buf) - 1);
              buf[sizeof(buf) - 1] = '\0';
              if (ImGui::InputText(fieldName, buf, sizeof(buf)))
              {
                  dataEntry->stringVal = buf;
                  changed = true;
              }
              ImGui::NextColumn();
              break;
          }
          case STRUCT_ENTRY:
          {
              changed = this->ImguiDisplayStructDataEntry(dataEntry->name, dataEntry->structVal, scene, ent, isComponent);
              break;
          }
        }
        return changed;
    }

    // Diplays the data entry for a struct, and indicates whether any data has been changed.
    // Only reads from the data.
    bool ImguiDisplayStructDataEntry(std::string name, std::vector<DataEntry*> dataEntries, Scene &scene, EntityID ent, b32 isComponent)
    {
        bool changed = false;

        if (isComponent)
        {
            std::string entityComponentUniqueName = std::to_string(ent) + name;
            ImGui::PushID(entityComponentUniqueName.c_str());
        }
        
        const char *nodeName = name.c_str();
        if (ImGui::TreeNode(nodeName))
        {
            for (DataEntry *dataEntry : dataEntries)
            {
                changed |= this->ImguiDisplayDataEntry(dataEntry, scene, ent, false);
            }
            ImGui::TreePop();
        }

        if (isComponent)
        {
            ImGui::PopID();
        }
        return changed;
    }

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

        // NOTE(marvin): Proof of concept for interactive tree view for components of a single entity

        // Create a transparent, non-interactive overlay window
        ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | 
        ImGuiWindowFlags_NoBackground;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        ImGui::Begin("Overlay", nullptr, window_flags);

        // TODO(marvin): Make name editable.
        // NOTE(marvin): Entities list
        if (ImGui::BeginListBox("Entities"))
        {
            for (Scene::EntityEntry entityEntry : scene->entities)
            {
                EntityID entityID = entityEntry.id;
                NameComponent *maybeNameComponent = scene->Get<NameComponent>(entityID);
                if (maybeNameComponent)
                {
                    NameComponent *nameComponent = maybeNameComponent;
                    const char *entityName = (nameComponent->name).c_str();
                    std::string entityIDString = std::to_string(entityID);
                    
                    ImGui::PushID(entityIDString.c_str());
                    const bool isSelected = (entityID == this->selectedEntityID);

                    if (isSelected)
                    {
                        // NOTE(marvin): Pulled this number out of my ass.
                        char buf[256];
                        strncpy(buf, entityName, sizeof(buf) - 1);
                        buf[sizeof(buf) - 1] = '\0';

                        if (ImGui::InputText(("##" + entityIDString).c_str(), buf, sizeof(buf)))
                        {
                            nameComponent->name = buf;
                        }
                    }
                    else
                    {
                        if (ImGui::Selectable(entityName, isSelected))
                        {
                            selectedEntityID = entityID;
                        }
                
                        ImGui::IsItemHovered();
                    }
                    ImGui::PopID();
                }
            
            }
            ImGui::EndListBox();
        }

        // NOTE(marvin): Add new entity.
        if (ImGui::Button("New Entity"))
        {
            // TODO(marvin): Probably want a helper that creates the entity through this ritual. Common with the one in scene_loader::LoadScene, but that one doesn't assign a Transform3D.
            EntityID newEntityID = scene->NewEntity();

            // TODO(marvin): Will there be problems if there is an entity with that name already?
            std::string entityName = "New Entity";
            entityIds[entityName] = newEntityID;
            NameComponent* nameComp = scene->Assign<NameComponent>(newEntityID);
            nameComp->name = entityName;
            scene->Assign<Transform3D>(newEntityID);
            
            selectedEntityID = newEntityID;
        }

        // NOTE(marvin): Component interactive tree view
        for (ComponentID componentID : EntityView(*scene, selectedEntityID))
        {
            ComponentInfo compInfo = compInfos[componentID];
            if (compInfo.name == NAME_COMPONENT)
            {
                continue;
            }

            DataEntry *dataEntry = compInfo.readFunc(*scene, selectedEntityID);
            bool changed = ImguiDisplayDataEntry(dataEntry, *scene, selectedEntityID, true);
            if (changed)
            {
                s32 val = compInfo.writeFunc(*scene, selectedEntityID, dataEntry);
                if (val != 0)
                {
                    printf("failed to write component");
                }
            }
            delete dataEntry;
        }

        // NOTE(marvin): Save scene button
        if (ImGui::Button("Save Scene"))
        {
            SaveCurrentScene(*scene);
        }
        
        ImGui::End();
    }
};
