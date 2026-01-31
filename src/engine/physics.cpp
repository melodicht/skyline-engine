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
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <cmath>

#include <meta_definitions.h>
#include <skl_math_types.h>
#include <engine.h>
#include <physics.h>
#include <engine_components.h>
#include <scene_view.h>

constexpr siz TEMPORARY_MEMORY_SIZE = Megabytes(1);

SKLPhysicsSubSystemBuffer InitPhysicsSubsystemBuffer(u32 count)
{
    SKLPhysicsSubSystemBuffer result = {};
    void* base = allocator.Allocate(sizeof(skl_physics_subsystem_t*) * count);
    result.subsystems = static_cast<skl_physics_subsystem_t**>(base);
    result.count = count;
    return result;
}

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
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return "SKLBroadPhaseLayer";
    }
#endif
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

JPH::Vec3 OurToJoltCoordinateSystem(glm::vec3 ourVec3)
{
    f32 rx = -ourVec3.y;
    f32 ry = ourVec3.z;
    f32 rz = ourVec3.x;
    JPH::Vec3 result{rx, ry, rz};
    return result;
}

glm::vec3 JoltToOurCoordinateSystem(JPH::Vec3 joltVec3)
{
    f32 rx = joltVec3.GetZ();
    f32 ry = -joltVec3.GetX();
    f32 rz = joltVec3.GetY();
    glm::vec3 result{rx, ry, rz};
    return result;
}

JPH::Vec3 LerpJPHVec3(JPH::Vec3 a, JPH::Vec3 b, f32 blendFactor)
{
    JPH::Vec3Arg result{
        std::lerp(a.GetX(), b.GetX(), blendFactor),
        std::lerp(a.GetY(), b.GetY(), blendFactor),
        std::lerp(a.GetZ(), b.GetZ(), blendFactor),
    };
    return result;
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
 * SUBSYSTEM
 */

local void UpdateSubsystems(SKLPhysicsSubSystemBuffer buffer, SKLPhysicsSystem* sklPhysicsSystem, SYSTEM_VTABLE_ON_UPDATE_PARAMS)
{
    for (u32 index = 0; index < buffer.count; ++index)
    {
        skl_physics_subsystem_t* subsystem = buffer.subsystems[index];
        subsystem(sklPhysicsSystem, SYSTEM_VTABLE_ON_UPDATE_PASS);
    }
}


/**
 * SYSTEM DEFINITION
 */

SKLPhysicsSystem::SKLPhysicsSystem() : SYSTEM_SUPER(SKLPhysicsSystem)
{
    this->preUpdateSubsystemBuffer = {};
    this->postUpdateSubsystemBuffer = {};
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
    // NOTE(marvin): Initialising static boxes
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

            JPH::Vec3 position = OurToJoltCoordinateSystem(t->GetWorldPosition());
            JPH::BodyCreationSettings bodyCreationSettings{shape, position,
                                                           JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layer::NON_MOVING};
            JPH::Body *body = bodyInterface.CreateBody(bodyCreationSettings);
            bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);

            sb->initialized = true;
        }
    }

    UpdateSubsystems(this->preUpdateSubsystemBuffer, this, SYSTEM_VTABLE_ON_UPDATE_PASS);

    u32 collisionSteps = 1;
    this->physicsSystem->Update(deltaTime, collisionSteps, this->allocator, this->jobSystem);

    UpdateSubsystems(this->postUpdateSubsystemBuffer, this, SYSTEM_VTABLE_ON_UPDATE_PASS);
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

#if 0
    if (!firstTime)
    {
        // NOTE(marvin): This isn't necessary, could just let it leak
        // since this is a development feature. Did this cause I
        // thought it had to do with a bug but it didn't. Can't delete
        // because the destructor is virtual, which is lost after a
        // hot reload. We take advantage of the fact that from the
        // Jolt source code, we can see that the new/delete simply
        // forwards the pointer as-is to the allocation functions.
#if defined(JPH_COMPILER_MINGW) && JPH_CPU_ARCH_BITS == 32
        JPH::AlignedFree(this->physicsSystem);
        JPH::AlignedFree(this->broadPhaseLayer);
        JPH::AlignedFree(this->objectVsBoradPhaseLayerFilter);
        JPH::AlignedFree(this->objectLayerPairFilter);
        JPH::AlignedFree(this->jobSystem);
        JPH::AlignedFree(this->allocator);
#else
        JPH::Free(this->physicsSystem);
        JPH::Free(this->broadPhaseLayer);
        JPH::Free(this->objectVsBroadPhaseLayerFilter);
        JPH::Free(this->objectLayerPairFilter);
        JPH::Free(this->jobSystem);
        JPH::Free(this->allocator);
#endif
    }

#endif

    JPH::JobSystemThreadPool *jobSystem = new JPH::JobSystemThreadPool(maxPhysicsJobs, maxPhysicsBarriers, numPhysicsThreads);

    // NOTE(marvin): This is not our ECS system! Jolt happened to name it System as well.
    JPH::PhysicsSystem* physicsSystem = new JPH::PhysicsSystem();

    if (!this->userOverrideLayers)
    {
        this->broadPhaseLayer = new SklBroadPhaseLayer();
        this->objectVsBroadPhaseLayerFilter = new SklObjectVsBroadPhaseLayerFilter();
        this->objectLayerPairFilter = new SklObjectLayerPairFilter();
    }
    
    physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                        *this->broadPhaseLayer, *this->objectVsBroadPhaseLayerFilter,
                        *this->objectLayerPairFilter);

    this->physicsSystem = physicsSystem;
    this->jobSystem = jobSystem;

#if SKL_SLOW
    if (!firstTime)
    {
        ASSERT(this->allocator->GetUsage() == 0);
    }
#endif
    this->allocator = new JPH::TempAllocatorImpl(TEMPORARY_MEMORY_SIZE);
}

void SKLPhysicsSystem::InitializeLayers(JPH::BroadPhaseLayerInterface* broadPhaseLayer, JPH::ObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseLayerFilter, JPH::ObjectLayerPairFilter* objectLayerPairFilter)
{
    this->userOverrideLayers = true;

    this->broadPhaseLayer = broadPhaseLayer;
    this->objectVsBroadPhaseLayerFilter = objectVsBroadPhaseLayerFilter;
    this->objectLayerPairFilter = objectLayerPairFilter;
}

void SKLPhysicsSystem::InitializeSubSystems(SKLPhysicsSubSystemBuffer preUpdateSubsystemBuffer,
                                            SKLPhysicsSubSystemBuffer postUpdateSubsystemBuffer)
{
    this->preUpdateSubsystemBuffer = preUpdateSubsystemBuffer;
    this->postUpdateSubsystemBuffer = postUpdateSubsystemBuffer;
}
