#pragma once

#include <engine.h>
#include <scene.h>
#include <engine_components.h>
#include <system_registry.h>
#include <skl_types.h>

namespace JPH
{
    class PhysicsSystem;
    class JobSystem;
    class CharacterVirtual;
    class Vec3;
}

// TODO(marvin): Major differences between physics subsystem and an ecs system. Should there abe any differences? Biggest once is that physics subsystem is not a struct and cannot hold information of its own. Doesn't have initialize, though there could be initialization checks in there that does initialization first-time.

class SKLPhysicsSystem;

#define SKL_PHYSICS_SUBSYSTEM(name) void name(SKLPhysicsSystem* sklPhysicsSystem, SYSTEM_VTABLE_ON_UPDATE_PARAMS)
typedef SKL_PHYSICS_SUBSYSTEM(skl_physics_subsystem_t);

struct SKLPhysicsSubSystemBuffer
{
    skl_physics_subsystem_t** subsystems;
    u32 count;
};

SKLPhysicsSubSystemBuffer InitPhysicsSubsystemBuffer(u32 count);

class SKLPhysicsSystem : public System
{
public:
    JPH::PhysicsSystem* physicsSystem;
    JPH::BroadPhaseLayerInterface* broadPhaseLayer;
    JPH::ObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseLayerFilter;
    JPH::ObjectLayerPairFilter* objectLayerPairFilter;
    JPH::JobSystem* jobSystem;
    JPH::TempAllocatorImpl* allocator;

    b32 userOverrideLayers;

    SKLPhysicsSubSystemBuffer preUpdateSubsystemBuffer;
    SKLPhysicsSubSystemBuffer postUpdateSubsystemBuffer;
    
    SKLPhysicsSystem();

    ~SKLPhysicsSystem();

    SYSTEM_ON_UPDATE();

    void Initialize(b32 firstTime = false);

    // Must be called before Initialize for it to take effect.
    void InitializeLayers(JPH::BroadPhaseLayerInterface* broadPhaseLayer_, JPH::ObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseLayerFilter, JPH::ObjectLayerPairFilter* objectLayerPairFilter_);

    // Must be called before Initialize for it to take effect.
    void InitializeSubSystems(SKLPhysicsSubSystemBuffer preUpdateSubsystemBuffer,
                              SKLPhysicsSubSystemBuffer postUpdateSubsystemBuffer);
};

JPH::Vec3 OurToJoltCoordinateSystem(glm::vec3 ourVec3);

glm::vec3 JoltToOurCoordinateSystem(JPH::Vec3 joltVec3);

JPH::Vec3 LerpJPHVec3(JPH::Vec3 a, JPH::Vec3 b, f32 blendFactor);

