class GravitySystem : public System
{
    void OnUpdate(Scene *scene, f32 deltaTime)
    {
        for (EntityID ent: SceneView<Rigidbody, GravityComponent>(*scene))
        {
            Rigidbody *rb = scene->Get<Rigidbody>(ent);
            GravityComponent *gc = scene->Get<GravityComponent>(ent);

            rb->v_y -= gc->strength;
        }
    }
};

// Scans for collision of a single component
// and edits trajectory of ball otherwise
void scanCollision(CircleCollider *checkCollider, Rigidbody *accessRigid, Transform3D *accessTransform, Scene &accessScene)
{
    for (EntityID ent: SceneView<Transform3D, Rigidbody, CircleCollider>(accessScene))
    {
        Transform3D *t = accessScene.Get<Transform3D>(ent);
        Rigidbody *rb = accessScene.Get<Rigidbody>(ent);
        CircleCollider *cc = accessScene.Get<CircleCollider>(ent);
        if (rb != accessRigid)
        {
            double diffX = t->position.y - accessTransform->position.y;
            double diffY = t->position.z - accessTransform->position.z;
            double distance = sqrt(diffX * diffX + diffY * diffY);
            if (distance < cc->radius + checkCollider->radius)
            {
                double normX = diffX / distance;
                double normY = diffY / distance;
                double thisSpeedMag = -sqrt(accessRigid->v_x * accessRigid->v_x + accessRigid->v_y * accessRigid->v_y);
                accessRigid->v_x = normX * thisSpeedMag;
                accessRigid->v_y = normY * thisSpeedMag;
                double speedMag = sqrt(rb->v_x * rb->v_x + rb->v_y * rb->v_y);
                rb->v_x = normX * speedMag;
                rb->v_y = normY * speedMag;
            }
        }
    }
}

class CollisionSystem : public System
{
    void OnUpdate(Scene *scene, f32 deltaTime)
    {
        // Forward movement, collision, rendering
        for (EntityID ent: SceneView<Transform3D, Rigidbody, CircleCollider>(*scene))
        {
            Transform3D *t = scene->Get<Transform3D>(ent);
            Rigidbody *rb = scene->Get<Rigidbody>(ent);
            CircleCollider *cc = scene->Get<CircleCollider>(ent);
            float radius = cc->radius;

            // Not framerate independent for simpler collision logic.
            t->position.y += rb->v_x;
            t->position.z += rb->v_y;

            // Collision check x-axis
            if ((t->position.y - radius) < (WINDOW_WIDTH / -2.0f) || (t->position.y + radius) > (WINDOW_WIDTH / 2.0f))
            {
                rb->v_x *= -1;
            }

            // Collision check y-axis
            if ((t->position.z - radius) < (WINDOW_HEIGHT / -2.0f) || (t->position.z + radius) > (WINDOW_HEIGHT / 2.0f))
            {
                rb->v_y *= -1;
            }

            scanCollision(cc, rb, t, *scene);
        }
    }
};

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

bool exists = false;
class RenderSystem : public System
{

    void OnStart(Scene *scene)
    {
        RenderPipelineInitInfo initDesc {};

        InitPipelines(initDesc);
    }

    void OnUpdate(Scene *scene, f32 deltaTime)
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

            dirLights.push_back({l->lightID, *lTransform, l->diffuse, l->specular});
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

            spotLights.push_back({l->lightID, *lTransform, l->diffuse, l->specular,
                                  l->innerCone, l->outerCone, l->range});
        }

        std::vector<PointLightRenderInfo> pointLights;
        for (EntityID ent: SceneView<PointLight, Transform3D>(*scene))
        {
            PointLight *l = scene->Get<PointLight>(ent);
            if (l->lightID == -1)
            {
                l->lightID = AddPointLight();
                std::cout << l->lightID << std::endl;
            }

            Transform3D *lTransform = scene->Get<Transform3D>(ent);

            if (l->lightID == 2) {
                pointLights.push_back({l->lightID, *lTransform, l->diffuse, l->specular,
                        l->constant, l->linear, l->quadratic, l->maxRange});
            }
        }

        std::vector<MeshRenderInfo> meshInstances;
        for (EntityID ent: SceneView<MeshComponent, ColorComponent, Transform3D>(*scene))
        {
            Transform3D *t = scene->Get<Transform3D>(ent);
            glm::mat4 model = GetTransformMatrix(t);
            ColorComponent *c = scene->Get<ColorComponent>(ent);
            MeshComponent *m = scene->Get<MeshComponent>(ent);
            MeshID mesh = m->mesh;
            meshInstances.push_back({model, {c->r, c->g, c->b}, mesh});
        }

        RenderFrameInfo sendState{
            .cameraTransform = *cameraTransform,
            .meshes = meshInstances, 
            .dirLights = dirLights,
            .spotLights = spotLights,
            .pointLights = pointLights,
            .cameraFov = camera->fov,
            .cameraNear = camera->near,
            .cameraFar = camera->far
        };
        
        RenderUpdate(sendState);
    }
};

class MovementSystem : public System
{
    void OnUpdate(Scene *scene, f32 deltaTime)
    {
        for (EntityID ent: SceneView<FlyingMovement, Transform3D>(*scene))
        {
            FlyingMovement *f = scene->Get<FlyingMovement>(ent);
            Transform3D *t = scene->Get<Transform3D>(ent);

            t->rotation.z += mouseDeltaX * f->turnSpeed;
            t->rotation.y += mouseDeltaY * f->turnSpeed;
            t->rotation.y = std::min(std::max(t->rotation.y, -90.0f), 90.0f);

            if (keysDown["W"])
            {
                t->position += GetForwardVector(t) * f->moveSpeed * deltaTime;
            }

            if (keysDown["S"])
            {
                t->position -= GetForwardVector(t) * f->moveSpeed * deltaTime;
            }

            if (keysDown["D"])
            {
                t->position += GetRightVector(t) * f->moveSpeed * deltaTime;
            }

            if (keysDown["A"])
            {
                t->position -= GetRightVector(t) * f->moveSpeed * deltaTime;
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

    void OnUpdate(Scene *scene, f32 deltaTime)
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
                if (RandInBetween(0.0f, 1.0f) > 0.9375f)
                {
                    // Build antenna
                    f32 antennaHeight = RandInBetween(antennaHeightMin, antennaHeightMax);
                    BuildPart(scene, ent, t, cuboidMesh, {antennaWidth, antennaWidth, antennaHeight});
                    t->position.z -= antennaWidth / 2;

                    if (pointLightCount < 256)
                    {
                        EntityID pointLight = scene->NewEntity();
                        Transform3D* pointTransform = scene->Assign<Transform3D>(pointLight);
                        *pointTransform = *t;
                        pointTransform->position.z += antennaHeight / 2;
                        pointTransform->scale = {10.0f, 10.0f, 10.0f};
                        //
                        MeshComponent *m = scene->Assign<MeshComponent>(pointLight);
                        m->mesh = cuboidMesh;
                        ColorComponent *c = scene->Assign<ColorComponent>(pointLight);
                        c->r = 1.0f;
                        c->g = 1.0f;
                        c->b = 1.0f;
                        //
                        PointLight* pointLightComponent = scene->Assign<PointLight>(pointLight);
                        f32 red = RandInBetween(0.8, 1.0);
                        pointLightComponent->diffuse = {red, 0.6, 0.25};
                        pointLightComponent->specular = {red, 0.6, 0.25};
                        pointLightComponent->constant = 1;
                        pointLightComponent->linear = 0.0005;
                        pointLightComponent->quadratic = 0.00005;
                        pointLightComponent->maxRange = 1000;

                        pointLightCount++;

                        LOG(pointLightCount);
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

                    t->rotation.z += glm::degrees(angle);
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
                    BuildPart(scene, ent, t, trapMesh, {plane->length, plane->width, trapHeight});

                    EntityID newPlane = scene->NewEntity();
                    Transform3D *newT = scene->Assign<Transform3D>(newPlane);
                    Plane *p = scene->Assign<Plane>(newPlane);
                    *newT = *t;
                    newT->position.z += trapHeight / 2;
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
                    BuildPart(scene, ent, t, pyraMesh, {plane->length, plane->width, pyraHeight});

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
                    BuildPart(scene, ent, t, prismMesh, {plane->length, plane->width, prismHeight});

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
                    BuildPart(scene, ent, t, cuboidMesh, {plane->length, plane->width, cuboidHeight});

                    EntityID newPlane = scene->NewEntity();
                    Transform3D *newT = scene->Assign<Transform3D>(newPlane);
                    Plane *p = scene->Assign<Plane>(newPlane);
                    *newT = *t;
                    newT->position.z += cuboidHeight / 2;
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

                        t->position -= GetForwardVector(t) * ((old - plane->length) * 0.5f);
                        newT->position += GetForwardVector(newT) * ((old - p->length) * 0.5f);
                    }
                    else
                    {
                        // Split Y axis
                        f32 old = plane->width;
                        f32 divisible = plane->width - 16.0f;

                        plane->width = divisible * ratio;
                        p->width = divisible * (1.0f - ratio);

                        t->position -= GetRightVector(t) * ((old - plane->width) * 0.5f);
                        newT->position += GetRightVector(newT) * ((old - p->width) * 0.5f);
                    }
                }
            }
        }
    }

    void BuildPart(Scene *scene, EntityID ent, Transform3D *t, uint32_t mesh, glm::vec3 scale)
    {
        t->position.z += scale.z / 2;
        t->scale = scale;

        MeshComponent *m = scene->Assign<MeshComponent>(ent);
        m->mesh = mesh;
        ColorComponent *c = scene->Assign<ColorComponent>(ent);
        f32 shade = RandInBetween(0.25f, 0.75f);
        c->r = shade;
        c->g = shade;
        c->b = shade;
    }
};
