#include <city_builder.h>
#include <meta_definitions.h>
#include <scene.h>
#include <components.h>
#include <skl_math_utils.h>
#include <game.h>
#include <scene_view.h>

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

constexpr f32 antennaHeightMin = 128;
constexpr f32 antennaHeightMax = 192;
constexpr f32 antennaWidth = 8;

constexpr f32 cuboidHeightMin = 32;
constexpr f32 cuboidHeightMax = 96;

constexpr f32 trapHeightMin = 24;
constexpr f32 trapHeightMax = 48;

constexpr f32 roofHeightMin = 32;
constexpr f32 roofHeightMax = 64;

void BuilderSystem::Step(Scene *scene)
{

    for (EntityID ent : SceneView<Transform3D, Spin>(*scene))
    {
        Transform3D *t = scene->Get<Transform3D>(ent);
        Spin *s = scene->Get<Spin>(ent);
        t->AddLocalRotation({0, 0, s->speed * 1.25f});
    }

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
                BuildPart(scene, ent, t, assetUtils.LoadMeshAsset("cube"), {antennaWidth, antennaWidth, antennaHeight});
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
                    pointLightComponent->radius = 500.0f;
                    pointLightComponent->falloff = 2.0f;

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
                BuildPart(scene, ent, t, assetUtils.LoadMeshAsset("trap"), {plane->length, plane->width, trapHeight});

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
                BuildPart(scene, ent, t, assetUtils.LoadMeshAsset("pyra"), {plane->length, plane->width, pyraHeight});

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
                BuildPart(scene, ent, t, assetUtils.LoadMeshAsset("prism"), {plane->length, plane->width, prismHeight});

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
                BuildPart(scene, ent, t, assetUtils.LoadMeshAsset("cube"), {plane->length, plane->width, cuboidHeight});

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

void BuilderSystem::BuildPart(Scene *scene, EntityID ent, Transform3D *t, MeshAsset *mesh, glm::vec3 scale)
{
    t->AddLocalPosition({0, 0, scale.z / 2});
    t->SetLocalScale(scale);

    MeshComponent *m = scene->Assign<MeshComponent>(ent);
    m->mesh = mesh;
    f32 shade = RandInBetween(0.25f, 0.75f);
    m->color = {shade, shade, shade};
}

BuilderSystem::BuilderSystem(bool slowStep) : SYSTEM_SUPER(BuilderSystem)
{
    this->slowStep = slowStep;
}

MAKE_SYSTEM_MANUAL_VTABLE(BuilderSystem);

SYSTEM_ON_UPDATE(BuilderSystem)
{
    if (this->slowStep && this->timer > 0.0f)
    {
        this->timer -= deltaTime;
    }
    else
    {
        this->timer = 1.0f / this->rate;
        Step(scene);
    }
}
