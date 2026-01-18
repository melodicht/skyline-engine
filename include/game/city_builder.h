#pragma once

#include <scene.h>
#include <system_registry.h>

struct Transform3D;
struct MeshAsset;

class BuilderSystem : public System
{
private:
    bool slowStep = false;
    f32 timer = 2.0f; // Seconds until next step
    f32 rate = 0.5f;   // Steps per second

    u32 pointLightCount = 0;

    void Step(Scene *scene);

    void BuildPart(Scene *scene, EntityID ent, Transform3D *t, MeshAsset *mesh, glm::vec3 scale);

    BuilderSystem(bool slowStep);

    MAKE_SYSTEM_DECLARATIONS(BuilderSystem);
    
public:
    SYSTEM_ON_UPDATE();
};
