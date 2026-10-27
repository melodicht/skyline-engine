#pragma once

#include <scene.h>
#include <system_registry.h>

struct Transform3D;

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
    SYSTEM_ON_UPDATE(Scene &scene, GameInput &input, f32 deltaTime);
};

class PongBall2DSystem : public System
{
public:
    PongBall2DSystem();
    SYSTEM_ON_UPDATE(Scene &scene, f32 deltaTime);
};

class FightBall2DSystem : public System
{
public:
    FightBall2DSystem();
    SYSTEM_ON_UPDATE(Scene &scene, GameInput &input, f32 deltaTime);
};

class Fighter2DSystem : public System
{
public:
    Fighter2DSystem();
    SYSTEM_ON_UPDATE(Scene &scene, GameInput &input, f32 deltaTime);
};
