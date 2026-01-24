// Responsible for definitions used by the physics system, and also a
// conversion from our coordinate system to Jolt's.

// TODO(marvin): Should there be any Jolt definitions like these in engine space, should they only exist in user space?

// NOTE(marvin): Took Jolt includes from
// https://github.com/jrouwe/JoltPhysicsHelloWorld/blob/main/Source/HelloWorld.cpp

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <meta_definitions.h>
#include <skl_math_types.h>
#include <game.h>
#include <physics.h>
#include <components.h>
#include <utils.h>
#include <scene_view.h>

constexpr siz TEMPORARY_MEMORY_SIZE = Kilobytes(100);

/* From the Jolt 5.3.0 documentation:
  
   "A standard setup would be to have at least 2 broad phase layers:
   One for all static bodies (which is infrequently updated but is
   expensive to update since it usually contains most bodies) and one
   for all dynamic bodies (which is updated every simulation step but
   cheaper to update since it contains fewer objects)."
*/

// NOTE(marvin): JPH::ObjectLayer is a typedef for an integer, whereas JPH::BroadPhaseLayer is a class that needs to be constructed, hence the `= ...` vs `(...)`.

namespace Layer
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr u32 NUM_LAYERS = 2;
};

namespace BroadPhaseLayer
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr u32 NUM_LAYERS(2);
};

class SklBroadPhaseLayer final : public JPH::BroadPhaseLayerInterface
{
public:
    virtual u32 GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayer::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer objectLayer) const override
    {
        ASSERT(objectLayer < this->GetNumBroadPhaseLayers());
        // NOTE(marvin): Layer and BroadPhaseLayer maps 1:1, which is why we can do this.
        return JPH::BroadPhaseLayer(objectLayer);
    }

    virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return "SKLBroadPhaseLayer";
    }
};

// NOTE(marvin): See https://jrouwe.github.io/JoltPhysicsDocs/5.3.0/index.html#collision-detection

class SklObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
    {
        switch (layer1)
        {
            case Layer::NON_MOVING:
            {
                return layer2 == BroadPhaseLayer::MOVING;
            }
            case Layer::MOVING:
            {
                return true;
            }
            default:
            {
                return false;
            }
            
        }
    }
};

class SklObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        switch (layer1)
        {
            case Layer::NON_MOVING:
            {
                return layer2 == Layer::MOVING;
            }
            case Layer::MOVING:
            {
                return true;
            }
            default:
            {
                return false;
            }
            
        }
    }
};


inline JPH::Vec3 OurToJoltCoordinateSystem(glm::vec3 ourVec3)
{
    f32 rx = -ourVec3.y;
    f32 ry = ourVec3.z;
    f32 rz = ourVec3.x;
    JPH::Vec3 result{rx, ry, rz};
    return result;
}

inline glm::vec3 JoltToOurCoordinateSystem(JPH::Vec3 joltVec3)
{
    f32 rx = joltVec3.GetZ();
    f32 ry = -joltVec3.GetX();
    f32 rz = joltVec3.GetY();
    glm::vec3 result{rx, ry, rz};
    return result;
}

void SKLPhysicsSystem::MoveCharacterVirtual(JPH::CharacterVirtual *characterVirtual, JPH::Vec3 movementDirection, f32 moveSpeed, f32 deltaTime)
{
    JPH::Vec3 velocity = characterVirtual->GetLinearVelocity();
    JPH::Vec3Arg gravity{0, -9.81f, 0};
    velocity += gravity * deltaTime;
    velocity.SetX(0.0f);
    velocity.SetZ(0.0f);
    velocity += movementDirection * moveSpeed;
    characterVirtual->SetLinearVelocity(velocity);

    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    characterVirtual->ExtendedUpdate(deltaTime,
                                    gravity,
                                    settings,
                                    this->physicsSystem->GetDefaultBroadPhaseLayerFilter(Layer::MOVING),
                                    this->physicsSystem->GetDefaultLayerFilter(Layer::MOVING),
                                    {},
                                    {},
                                    *allocator);

    // TODO(marvin): Physics System update should happen in its own system.
    u32 collisionSteps = 1;
    this->physicsSystem->Update(deltaTime, collisionSteps, this->allocator, this->jobSystem);
}

void initializePlayerCharacter(PlayerCharacter *pc, JPH::PhysicsSystem *physicsSystem)
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

/**
 * JOLT ALLOCATION FUNCTIONS
 */

// NOTE(marvin): Using size_t instead of our own because that's what
// the Jolt signatures use.

void *JoltAlignedAllocate(size_t size, size_t alignment)
{
    void *result = allocator.AlignedAllocate(size, alignment);
    return result;
}

void JoltAlignedFree(void *block)
{
    allocator.AlignedFree(block);
}

void *JoltAllocate(size_t size)
{
    void *result = allocator.Allocate(size);
    return result;
}

void JoltFree(void *block)
{
    allocator.Free(block);
}

void *JoltReallocate(void *block, size_t oldSize, size_t newSize)
{
    void *result = allocator.Realloc(block, oldSize, newSize);
    return result;
}



/**
 * SYSTEM DEFINITION
 */

SKLPhysicsSystem::SKLPhysicsSystem() : SYSTEM_SUPER(SKLPhysicsSystem)
{
    this->Initialize(true);
}

SKLPhysicsSystem::~SKLPhysicsSystem()
{
    delete this->allocator;
    delete this->jobSystem;
    delete this->objectLayerPairFilter;
    delete this->objectVsBroadPhaseLayerFilter;
    delete this->broadPhaseLayer;
    delete this->physicsSystem;
}

MAKE_SYSTEM_MANUAL_VTABLE(SKLPhysicsSystem);

SYSTEM_ON_UPDATE(SKLPhysicsSystem)
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
        initializePlayerCharacter(pc, this->physicsSystem);
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
    MoveCharacterVirtual(cv, joltMovementDirection, moveSpeed, deltaTime);

    // Update player's transform from character virtual's position
    JPH::Vec3 joltPosition = cv->GetPosition();
    glm::vec3 position = JoltToOurCoordinateSystem(joltPosition);
    pt->SetLocalPosition(position);
}

// NOTE(marvin): The destructors are virtual, so we lose access to
// the destructors after a hot reload... Could try to deallocate
// the memory by hand if we can figure out what Jolt destructors
// are doing. Though that seems like a lot of work that's risky,
// and this is just a internal development feature, so we let it
// leak for now.
void SKLPhysicsSystem::Initialize(b32 firstTime)
{
    JPH::AlignedAllocate = JoltAlignedAllocate;
    JPH::AlignedFree = JoltAlignedFree;
    JPH::Allocate = JoltAllocate;
    JPH::Free = JoltFree;
    JPH::Reallocate = JoltReallocate;

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
    JPH::PhysicsSystem* physicsSystem = new JPH::PhysicsSystem();

    JPH::BroadPhaseLayerInterface* sklBroadPhaseLayer = new SklBroadPhaseLayer();
    JPH::ObjectVsBroadPhaseLayerFilter* sklObjectVsBroadPhaseLayerFilter = new SklObjectVsBroadPhaseLayerFilter();
    JPH::ObjectLayerPairFilter* sklObjectLayerPairFilter = new SklObjectLayerPairFilter();

    physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                        *sklBroadPhaseLayer, *sklObjectVsBroadPhaseLayerFilter,
                        *sklObjectLayerPairFilter);

    this->physicsSystem = physicsSystem;
    this->broadPhaseLayer = sklBroadPhaseLayer;
    this->objectVsBroadPhaseLayerFilter = sklObjectVsBroadPhaseLayerFilter;
    this->objectLayerPairFilter = sklObjectLayerPairFilter;
    this->jobSystem = jobSystem;

#if SKL_SLOW
    if (!firstTime)
    {
        Assert(this->allocator->GetUsage() == 0);
    }
#endif
    this->allocator = new JPH::TempAllocatorImpl(TEMPORARY_MEMORY_SIZE);
}
