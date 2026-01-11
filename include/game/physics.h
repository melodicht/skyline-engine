#pragma once

#include <scene.h>
#include <components.h>

namespace JPH
{
    class PhysicsSystem;
    class TempAllocatorImpl;
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

    void MoveCharacterVirtual(JPH::CharacterVirtual &characterVirtual, JPH::PhysicsSystem &physicsSystem,
                              JPH::Vec3 movementDirection, f32 moveSpeed, f32 deltaTime);

public:
    SKLPhysicsSystem();

    ~SKLPhysicsSystem();

    void OnStart(Scene *scene);

    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime);
};


