#include <string>
#include <array>

#include <meta_definitions.h>
#include <game.h>
#include <engine.h>
#include <city_builder.h>
#include <scene.h>
#include <scene_view.h>

#if MARVIN_GAME
#include <marvin_systems.h>
#endif

#include <movement.h>

#include <game_components.h>

#include <physics.h>
#include <utils.h>


// TODO(marvin): Not actually sure need all Jolt headers here, just want the damn thing to compile.

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

#if MARVIN_GAME
namespace MarvinLayer
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer PLAYER = 1;
    static constexpr JPH::ObjectLayer GRAVITY_BALLS = 2;
    static constexpr JPH::ObjectLayer BULLETS = 3;
    static constexpr u32 NUM_LAYERS = 4;
};

namespace MarvinBroadPhaseLayerNS
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer PLAYER(1);
    static constexpr JPH::BroadPhaseLayer GRAVITY_BALLS(2);
    static constexpr JPH::BroadPhaseLayer BULLETS(3);
    static constexpr u32 NUM_LAYERS(4);
};

class MarvinBroadPhaseLayer final : public JPH::BroadPhaseLayerInterface
{
public:
    virtual u32 GetNumBroadPhaseLayers() const override
    {
        return MarvinBroadPhaseLayerNS::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer objectLayer) const override
    {
        ASSERT(objectLayer < this->GetNumBroadPhaseLayers());
        return JPH::BroadPhaseLayer(objectLayer);
    }

    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return "MarvinBroadPhaseLayer";
    }
};

class MarvinObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
    {
        switch (layer1)
        {
          case MarvinLayer::NON_MOVING:
          {
              return layer2 == MarvinBroadPhaseLayerNS::PLAYER ||
              layer2 == MarvinBroadPhaseLayerNS::GRAVITY_BALLS ||
              layer2 == MarvinBroadPhaseLayerNS::BULLETS;
          }
          case MarvinLayer::PLAYER:
          {
              return layer2 == MarvinBroadPhaseLayerNS::NON_MOVING ||
              layer2 == MarvinBroadPhaseLayerNS::BULLETS;
          }
          case MarvinLayer::GRAVITY_BALLS:
          {
              return layer2 == MarvinBroadPhaseLayerNS::NON_MOVING ||
              layer2 == MarvinBroadPhaseLayerNS::BULLETS;
          }
          case MarvinLayer::BULLETS:
          {
              return layer2 == MarvinBroadPhaseLayerNS::NON_MOVING ||
              layer2 == MarvinBroadPhaseLayerNS::PLAYER ||
              layer2 == MarvinBroadPhaseLayerNS::GRAVITY_BALLS;
          }
          default:
          {
              return false;
          }
        }
    }
};

class MarvinObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        switch (layer1)
        {
          case MarvinLayer::NON_MOVING:
          {
              return layer2 == MarvinLayer::PLAYER ||
              layer2 == MarvinLayer::GRAVITY_BALLS ||
              layer2 == MarvinLayer::BULLETS;
          }
          case MarvinLayer::PLAYER:
          {
              return layer2 == MarvinLayer::NON_MOVING ||
              layer2 == MarvinLayer::BULLETS;
          }
          case MarvinLayer::GRAVITY_BALLS:
          {
              return layer2 == MarvinLayer::NON_MOVING ||
              layer2 == MarvinLayer::BULLETS;
          }
          case MarvinLayer::BULLETS:
          {
              return layer2 == MarvinLayer::NON_MOVING ||
              layer2 == MarvinLayer::PLAYER ||
              layer2 == MarvinLayer::GRAVITY_BALLS;
          }
          default:
          {
              return false;
          }
        }
    }
};

void InitializeGravityBall(GravityBall* gb, JPH::BodyInterface* bodyInterface,
                           JPH::Vec3Arg initialPosition, JPH::Vec3Arg direction,
                           f32 shootSpeed)
{
    // TODO(marvin): How to correspond our size to Jolt's size?
    f32 ballRadius = 3.0f;
    JPH::BodyCreationSettings ballSettings(new JPH::SphereShape(ballRadius),
                                      initialPosition,
                                      JPH::Quat::sIdentity(),
                                      JPH::EMotionType::Kinematic,
                                      MarvinLayer::GRAVITY_BALLS);

    JPH::Body* body = bodyInterface->CreateBody(ballSettings);
    JPH::BodyID bodyID = body->GetID();
    gb->body = body;
    gb->stage = gravityBallStage_growing;
    gb->life = 0;
    bodyInterface->AddBody(bodyID, JPH::EActivation::Activate);
    bodyInterface->SetLinearVelocity(body->GetID(), direction * shootSpeed);
    // NOTE(marvin): This is to allow us to go from body ID to the component.
    u64 gbAsU64 = reinterpret_cast<u64>(gb);
    bodyInterface->SetUserData(bodyID, gbAsU64);
}

SKLRay GetRayFromCamera(Transform3D* cameraTransform)
{
    SKLRay result = {};
    result.origin = cameraTransform->GetWorldPosition();
    result.direction = cameraTransform->GetForwardVector();
    return result;
}

void ActivateGravityBall(JPH::BodyInterface* bodyInterface, JPH::BodyID bodyID)
{
    // NOTE(marvin): To activate means to stop movement and growth, and begin the pull.
    JPH::Vec3 zeroVec3{0.0f, 0.0f, 0.0f};
    bodyInterface->SetLinearVelocity(bodyID, zeroVec3);
    u64 gbAsU64 = bodyInterface->GetUserData(bodyID);
    GravityBall* gb = reinterpret_cast<GravityBall*>(gbAsU64);
    gb->stage = gravityBallStage_prime;
    gb->primeCounter = 2.0f;
}

void BeginGravityBallDecay(GravityBall* gb)
{
    gb->stage = gravityBallStage_decaying;
}

void InitializePlayerCharacter(PlayerCharacter *pc, JPH::PhysicsSystem *physicsSystem)
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

// TODO(marvin): Should just inline body of MoveCharacterVirtual... 
local void MoveCharacterVirtual(JPH::CharacterVirtual* characterVirtual, PlayerCharacter* pc, Transform3D* playerTransform, SKLPhysicsSystem* sklPhysicsSystem, JPH::Vec3 movementDirection, f32 moveSpeed, b32 jumpHeld, Scene* scene, f32 deltaTime)
{
    JPH::Vec3 currentVelocity = characterVirtual->GetLinearVelocity();
    JPH::Vec3 currentVerticalVelocity{
        0.0f,
        currentVelocity.GetY(),
        0.0f,
    };

    // NOTE(marvin): Vertical movement, with variable jump height.
    f32 initialJumpVelocity = 10.0f;
    f32 gravityConstant = 30.0f;
    JPH::Vec3Arg gravityAcceleration{0, -gravityConstant, 0};
    f32 maxJumpHeight = 5.0f;
    f32 minJumpHeight = maxJumpHeight * 0.3f; // 30% of max height
    // NOTE(marvin): Derived from 0.5mv^2 = mgh, and solving for v. 
    f32 minJumpVelocity = sqrt(2.0f * gravityConstant * minJumpHeight);

    // NOTE(marvin): Jump initiation.
    if (!pc->isJumping && jumpHeld && characterVirtual->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround)
    {
        currentVerticalVelocity.SetY(initialJumpVelocity);
        pc->isJumping = true;
    }
    // NOTE(marvin): Player wants to cut the jump short.
    else if (pc->isJumping && !jumpHeld && currentVelocity.GetY() >= minJumpVelocity)
    {
        currentVerticalVelocity.SetY(minJumpVelocity);
        pc->isJumping = false;
    }
    else if (currentVelocity.GetY() < minJumpVelocity)
    {
        pc->isJumping = false;
    }

    JPH::Vec3 verticalVelocity = currentVerticalVelocity + (gravityAcceleration * deltaTime);

    // NOTE(marvin): Horizontal movement.
    JPH::Vec3 currentGroundedVelocity = currentVelocity;
    currentGroundedVelocity.SetY(0.0f);

    f32 sharpness = 15.0f;
    f32 myMoveSpeed = 8.5f;

    JPH::Vec3 targetGroundedVelocity = movementDirection * myMoveSpeed;
    JPH::Vec3 groundedVelocity = LerpJPHVec3(currentGroundedVelocity, targetGroundedVelocity,
                                             1 - std::exp(-sharpness * deltaTime));
    
    JPH::Vec3 velocity = groundedVelocity + verticalVelocity;

#if MARVIN_GAME
    // NOTE(marvin): The player's velocity is affected by the pull of gravity balls.
    JPH::Vec3 gravityBallAcceleration{0.0f, 0.0f, 0.0f};
    for (EntityID ent : SceneView<GravityBall, Transform3D>(*scene))
    {
        GravityBall* gb = scene->Get<GravityBall>(ent);
        Transform3D* t = scene->Get<Transform3D>(ent);

        JPH::Vec3 accelerationToAdd{0.0f, 0.0f, 0.0f};

        // NOTE(marvin): a = G / r^2, using inverse square law, where
        // masses are assumed to be 1, r is distance, and G is some
        // constant.

        f32 gravitationalConstant = 100.0f;
        f32 distance = glm::distance(playerTransform->GetWorldPosition(), t->GetWorldPosition());
        glm::vec3 ourVectorToGravityBall = glm::normalize(t->GetWorldPosition() - playerTransform->GetWorldPosition());
        JPH::Vec3 joltVectorToGravityBall = OurToJoltCoordinateSystem(ourVectorToGravityBall);

        f32 primeMultiplier = 10.0f;

        if (gb->stage == gravityBallStage_prime)
        {
            accelerationToAdd = joltVectorToGravityBall * primeMultiplier * (gravitationalConstant / (distance * distance));
        }
        else if (gb->stage == gravityBallStage_decaying)
        {
            accelerationToAdd = joltVectorToGravityBall * (gravitationalConstant / (distance * distance));
        }

        gravityBallAcceleration += accelerationToAdd;
    }
    velocity += gravityBallAcceleration * deltaTime;
#endif

    characterVirtual->SetLinearVelocity(velocity);

    JPH::PhysicsSystem* ps = sklPhysicsSystem->physicsSystem;

    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    characterVirtual->ExtendedUpdate(deltaTime,
                                    gravityAcceleration,
                                    settings,
                                    ps->GetDefaultBroadPhaseLayerFilter(MarvinLayer::PLAYER),
                                    ps->GetDefaultLayerFilter(MarvinLayer::PLAYER),
                                    {},
                                    {},
                                    *sklPhysicsSystem->allocator);
}


// sklPhysicsSystem, scene, input, deltaTime
SKL_PHYSICS_SUBSYSTEM(PlayerCharacterPreUpdate)
{
    SceneView<PlayerCharacter, Transform3D> playerView = SceneView<PlayerCharacter, Transform3D>(*scene);
    if (playerView.begin() == playerView.end())
    {
        return;
    }
    EntityID playerEnt = *playerView.begin();
    PlayerCharacter *pc = scene->Get<PlayerCharacter>(playerEnt);

    if (pc->characterVirtual == nullptr)
    {
        InitializePlayerCharacter(pc, sklPhysicsSystem->physicsSystem);
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
    b32 jumpHeld = OnHold(input, "Space");
    MoveCharacterVirtual(cv, pc, pt, sklPhysicsSystem, joltMovementDirection, moveSpeed, jumpHeld, scene, deltaTime);
}

// TODO(marvin): Note how pre and post update access basically same information. Perhaps wrap in struct so that information is stored and don't have to be recomputed?

SKL_PHYSICS_SUBSYSTEM(PlayerCharacterPostUpdate)
{
    SceneView<PlayerCharacter, Transform3D> playerView = SceneView<PlayerCharacter, Transform3D>(*scene);
    if (playerView.begin() == playerView.end())
    {
        return;
    }
    EntityID playerEnt = *playerView.begin();
    PlayerCharacter *pc = scene->Get<PlayerCharacter>(playerEnt);
    Transform3D *pt = scene->Get<Transform3D>(playerEnt);
    JPH::CharacterVirtual *cv = pc->characterVirtual;
    JPH::Vec3 joltPosition = cv->GetPosition();
    glm::vec3 position = JoltToOurCoordinateSystem(joltPosition);
    pt->SetLocalPosition(position);
}

SKL_PHYSICS_SUBSYSTEM(GravityBallsPreUpdate)
{
    SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(*scene);
    if (cameraView.begin() == cameraView.end())
    {
        return;
    }

    EntityID cameraEnt = *cameraView.begin();
    Transform3D* ct = scene->Get<Transform3D>(cameraEnt);
    JPH::BodyInterface* bodyInterface = &sklPhysicsSystem->physicsSystem->GetBodyInterface();
    
    for (EntityID ent : SceneView<GravityBall, Transform3D>(*scene))
    {
        GravityBall* gb = scene->Get<GravityBall>(ent);
        Transform3D* t = scene->Get<Transform3D>(ent);

        if (gb->body == nullptr)
        {
            f32 shootSpeed = 1.0f;
            JPH::Vec3 initialPosition = OurToJoltCoordinateSystem(t->GetLocalPosition());
            JPH::Vec3 direction = OurToJoltCoordinateSystem(t->GetLocalRotation());
            InitializeGravityBall(gb, bodyInterface, initialPosition, direction, shootSpeed);
        }

        if (gb->stage == gravityBallStage_growing)
        {
            gb->life += deltaTime;
        }
        else if (gb->stage == gravityBallStage_prime)
        {
            if (gb->primeCounter <= 0.0f)
            {
                BeginGravityBallDecay(gb);
            }
            gb->primeCounter -= deltaTime;
        }
        else if (gb->stage == gravityBallStage_decaying)
        {
            gb->life -= deltaTime;
        }

        // NOTE(marvin): Don't load the transform into body, let the body be the source of truth on position. Load body's position into transform.

        // NOTE(marvin): If the gravity ball runs into a static object, trigger it.
        if (gb->stage == gravityBallStage_growing)
        {
            JPH::BodyID bodyID = gb->body->GetID();
            const JPH::BodyLockInterface *bodyLockInterface = &sklPhysicsSystem->physicsSystem->GetBodyLockInterface();

            JPH::RMat44 gbJoltTransform;
            const JPH::Shape* gbShape;
            {
                JPH::BodyLockRead lock(*bodyLockInterface, bodyID);
                if (!lock.Succeeded())
                {
                    continue;
                }
                const JPH::Body* gbBody = &lock.GetBody();
                gbJoltTransform = gbBody->GetWorldTransform();
                gbShape = gbBody->GetShape();
            }
            JPH::CollideShapeSettings collideShapeSettings;
            JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

            sklPhysicsSystem->physicsSystem->GetNarrowPhaseQuery().CollideShape(gbShape,
                                                                    JPH::Vec3::sReplicate(1.0f),
                                                                    gbJoltTransform,
                                                                    collideShapeSettings,
                                                                    JPH::RVec3::sZero(),
                                                                    collector);

            b32 shouldActivateGravityBall = false;
            for (const JPH::CollideShapeResult& hit : collector.mHits)
            {
                JPH::BodyLockRead hitLock(*bodyLockInterface, hit.mBodyID2);
                if (hitLock.Succeeded())
                {
                    const JPH::Body* hitBody = &hitLock.GetBody();
                    if (hitBody->GetObjectLayer() == MarvinLayer::NON_MOVING)
                    {
                        shouldActivateGravityBall = true;
                        break;
                    }
                }
            }

            if (shouldActivateGravityBall)
            {
                ActivateGravityBall(bodyInterface, bodyID);
            }
        }
    }

    // NOTE(marvin): If user clicked on a gravity ball, trigger
    // it. Technically this could happen when ball collides with a
    // wall at the exact same frame, but it's fine.
    if (OnPress(input, "Mouse 1"))
    {
        SKLRay sklRay = GetRayFromCamera(ct);
        f32 rayLength = 1000.0f;
        JPH::Vec3 joltRayCastOrigin = OurToJoltCoordinateSystem(sklRay.origin);
        JPH::Vec3 joltRayCastDirection = OurToJoltCoordinateSystem(sklRay.direction);
        JPH::RayCast joltRayCast{joltRayCastOrigin, joltRayCastDirection * rayLength};
        JPH::RRayCast joltRRayCast{joltRayCast};
        JPH::RayCastResult joltRayCastResult;
        b32 hit = sklPhysicsSystem->physicsSystem->GetNarrowPhaseQuery().CastRay(joltRRayCast, joltRayCastResult);

        if (hit)
        {
            ActivateGravityBall(bodyInterface, joltRayCastResult.mBodyID);
        }
    }
}

SKL_PHYSICS_SUBSYSTEM(GravityBallsPostUpdate)
{
    for (EntityID ent : SceneView<GravityBall, Transform3D>(*scene))
    {
        GravityBall* gb = scene->Get<GravityBall>(ent);
        Transform3D* t = scene->Get<Transform3D>(ent);
        JPH::Vec3 joltPosition = gb->body->GetPosition();
        glm::vec3 position = JoltToOurCoordinateSystem(joltPosition);
        t->SetLocalPosition(position);
    }
}

void InitializeSKLPhysicsStuff(SKLPhysicsSystem* sklPhysicsSystem)
{
    JPH::BroadPhaseLayerInterface* sklBroadPhaseLayer = new MarvinBroadPhaseLayer();
    JPH::ObjectVsBroadPhaseLayerFilter* sklObjectVsBroadPhaseLayerFilter = new MarvinObjectVsBroadPhaseLayerFilter();
    JPH::ObjectLayerPairFilter* sklObjectLayerPairFilter = new MarvinObjectLayerPairFilter();
    sklPhysicsSystem->InitializeLayers(sklBroadPhaseLayer, sklObjectVsBroadPhaseLayerFilter, sklObjectLayerPairFilter);

    SKLPhysicsSubSystemBuffer preUpdateSubsystemBuffer = InitPhysicsSubsystemBuffer(2);
    preUpdateSubsystemBuffer.subsystems[0] = &PlayerCharacterPreUpdate;
    preUpdateSubsystemBuffer.subsystems[1] = &GravityBallsPreUpdate;
    SKLPhysicsSubSystemBuffer postUpdateSubsystemBuffer = InitPhysicsSubsystemBuffer(2);
    postUpdateSubsystemBuffer.subsystems[0] = &PlayerCharacterPostUpdate;
    postUpdateSubsystemBuffer.subsystems[1] = &GravityBallsPostUpdate;
    sklPhysicsSystem->InitializeSubSystems(preUpdateSubsystemBuffer, postUpdateSubsystemBuffer);
}
#endif

void OnGameStart(GameState* gameState, GameMemory* gameMemory)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

#if MARVIN_GAME
    scene.CreateVariableTimestepSystem<GravityBallsSystem>();
#endif
    scene.CreateSemifixedTimestepSystem<MovementSystem>();
    scene.CreateSemifixedTimestepSystem<BuilderSystem>(slowStep);

#if MARVIN_GAME
    SKLPhysicsSystem* sklPhysicsSystem = static_cast<SKLPhysicsSystem*>(gameMemory->sklPhysicsSystem);
    InitializeSKLPhysicsStuff(sklPhysicsSystem);
#endif

    assetUtils.LoadSkyboxAsset({"YokohamaSkybox/posx", "YokohamaSkybox/negx", "YokohamaSkybox/posy", "YokohamaSkybox/negy", "YokohamaSkybox/posz", "YokohamaSkybox/negz"});
}

void OnGameLoad(GameMemory* gameMemory)
{
#if MARVIN_GAME
    SKLPhysicsSystem* sklPhysicsSystem = static_cast<SKLPhysicsSystem*>(gameMemory->sklPhysicsSystem);
    if (sklPhysicsSystem)
    {
        InitializeSKLPhysicsStuff(sklPhysicsSystem);
    }
#endif
}

void OnEditorStart(GameState* gameState) {}
