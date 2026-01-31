#pragma once

#include <meta_definitions.h>
#include <component_registry.h>

struct FlyingMovement
{
    f32 moveSpeed = 5;
    f32 turnSpeed = 0.1;
};
SERIALIZE(FlyingMovement, moveSpeed, turnSpeed)
COMPONENT(FlyingMovement)

struct HorizontalLook
{
    f32 turnSpeed = 0.1f;
};
SERIALIZE(HorizontalLook, turnSpeed)
COMPONENT(HorizontalLook)

struct VerticalLook
{
    f32 turnSpeed = 0.1;
};
SERIALIZE(VerticalLook, turnSpeed)
COMPONENT(VerticalLook)


struct BuilderPlane
{
    f32 width = 1;
    f32 length = 1;
};
SERIALIZE(BuilderPlane, width, length)
COMPONENT(BuilderPlane)

struct Spin
{
    f32 speed = 20.0f;
};
SERIALIZE(Spin, speed)
COMPONENT(Spin)

#ifdef MARVIN_GAME

enum GravityBallStage
{
    gravityBallStage_growing,
    gravityBallStage_prime,
    gravityBallStage_decaying,
};

namespace JPH
{
    class Body;
}

struct GravityBall
{
    JPH::Body* body = nullptr;
    f32 life;
    f32 primeCounter;
    GravityBallStage stage = gravityBallStage_growing;
};
SERIALIZE(GravityBall)
COMPONENT(GravityBall)
#endif
