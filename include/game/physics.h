#pragma once

#include <scene.h>
#include <components.h>
#include <system_registry.h>
#include <skl_types.h>

namespace JPH
{
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystem;
    class CharacterVirtual;
    class Vec3;
}

class SklJoltAllocator final : public JPH::TempAllocator
{
private:
    MemoryArena arena;
public:
    JPH_OVERRIDE_NEW_DELETE
    
    explicit SklJoltAllocator(MemoryArena *remainingArena);

    virtual void *Allocate(u32 requestedSize) override;

    virtual void Free(void *address, u32 size) override;
};

class SKLPhysicsSystem : public System
{
private:
    JPH::PhysicsSystem *physicsSystem;
    SklJoltAllocator *allocator;
    JPH::JobSystem *jobSystem;

    void MoveCharacterVirtual(JPH::CharacterVirtual *characterVirtual,
                              JPH::Vec3 movementDirection, f32 moveSpeed, f32 deltaTime);

public:
    SKLPhysicsSystem(MemoryArena *remainingArena);

    ~SKLPhysicsSystem();

    SYSTEM_ON_UPDATE();
};
