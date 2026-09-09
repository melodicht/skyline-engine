#pragma once

#include <meta_definitions.h>
#include <component_registry.h>

struct PongMovement2D
{
    // World units per second. Bounds constrain the paddle center.
    f32 moveSpeed = 5;

    f32 topBoundY = 100;
    f32 bottomBoundY = 0;

    std::string upButton = "W";
    std::string downButton = "S";
};
SERIALIZE(PongMovement2D, moveSpeed, topBoundY, bottomBoundY, upButton, downButton)
COMPONENT(PongMovement2D)

struct PongBall2D
{
    // World units per second. Bounds constrain the ball center.
    f32 xVel = 0;
    f32 yVel = 0;

    f32 leftBound = -100.0f;
    f32 rightBound = 100.0f;

    f32 upBound = 100.0f;
    f32 downBound = -100.0f;
};
SERIALIZE(PongBall2D, xVel, yVel, leftBound, rightBound, upBound, downBound)
COMPONENT(PongBall2D)

struct FightBall2D
{
    f32 xVel = 0;
    f32 yVel = 0;

    f32 leftBound = -100.0f;
    f32 rightBound = 100.0f;

    f32 upBound = 100.0f;
    f32 downBound = -100.0f;
};
SERIALIZE(FightBall2D, xVel, yVel, leftBound, rightBound, upBound, downBound)
COMPONENT(FightBall2D)
