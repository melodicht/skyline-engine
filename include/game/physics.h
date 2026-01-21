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
    JPH::PhysicsSystem *physicsSystem;
    JPH::TempAllocatorImpl *allocator;
    JPH::JobSystem *jobSystem;

    void MoveCharacterVirtual(JPH::CharacterVirtual *characterVirtual,
                              JPH::Vec3 movementDirection, f32 moveSpeed, f32 deltaTime);

public:
    SKLPhysicsSystem();

    ~SKLPhysicsSystem();

    SYSTEM_ON_UPDATE();

    void RefreshTempAllocator();
};

