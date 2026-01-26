#pragma once

#include <scene.h>
#include <components.h>
#include <system_registry.h>
#include <skl_types.h>

namespace JPH
{
    class PhysicsSystem;
    class JobSystem;
    class CharacterVirtual;
    class Vec3;
}

class SKLPhysicsSystem : public System
{
private:
    JPH::PhysicsSystem* physicsSystem;
    JPH::BodyInterface* bodyInterface;
    JPH::BroadPhaseLayerInterface* broadPhaseLayer;
    JPH::ObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseLayerFilter;
    JPH::ObjectLayerPairFilter* objectLayerPairFilter;
    JPH::JobSystem* jobSystem;
    JPH::TempAllocatorImpl* allocator;

    // NOTE(marvin): Defined by the character having enough velocity
    // to reach to minimum jump height.
    b32 isJumping;

#if MARVIN_GAME
    b32 triggerWasDown;
#endif

    void MoveCharacterVirtual(JPH::CharacterVirtual *characterVirtual,
                              JPH::Vec3 movementDirection, f32 moveSpeed, b32 jumpHeld,
                              f32 deltaTime);

public:
    SKLPhysicsSystem();

    ~SKLPhysicsSystem();

    SYSTEM_ON_UPDATE();

    void Initialize(b32 firstTime = false);
};

