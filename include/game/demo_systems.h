#pragma once

#include <scene.h>
#include <system_registry.h>
#include <engine_components.h>
#include <game_components.h>

inline constexpr InputId InputPaddleUp = GetInputId("PadUp");
inline constexpr InputId InputPaddleDown = GetInputId("PadDown");
inline constexpr InputId InputFighterLeft = GetInputId("FightL");
inline constexpr InputId InputFighterRight = GetInputId("FightR");
inline constexpr InputId InputFighterAttack = GetInputId("Attack");
inline constexpr InputId InputFighterDash = GetInputId("Dash");
inline constexpr InputId InputReset = GetInputId("Reset");

class RectCollision2DSystem : public System
{
public:
    RectCollision2DSystem();
    SYSTEM_ON_UPDATE(Scene &scene);
};

class PongMovement2DSystem : public System
{
public:
    PongMovement2DSystem();
    COMPONENTS_ON_UPDATE(const PlayerInput &input, PongMovement2D &pongMovement, Transform3D &transform, f32 deltaTime);
};

class PongBall2DSystem : public System
{
public:
    PongBall2DSystem();
    COMPONENTS_ON_UPDATE(Scene &scene, PongBall2D &ball, CollisionBox2D &box, Transform3D &transform, f32 deltaTime);
};

class FightBall2DSystem : public System
{
public:
    FightBall2DSystem();
    SYSTEM_ON_UPDATE(Scene &scene, std::span<PlayerData> playerData, const InputSpecification &inputSpecification, f32 deltaTime);
};

class Fighter2DSystem : public System
{
public:
    Fighter2DSystem();
    COMPONENTS_ON_UPDATE(const PlayerInput &input, Scene &scene, EntityID entityId, Fighter2D &fighter, CollisionBox2D &fighterBox, MeshComponent &fighterMesh, Transform3D &fighterTransform, f32 deltaTime);
};
