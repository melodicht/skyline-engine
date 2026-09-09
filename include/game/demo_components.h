#pragma once

#include <glm/vec3.hpp>
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
    f32 vertVel = 0;
    f32 horVel = 0;
    f32 gravity = 9.8;
    f32 floor = 0;

    f32 vel = 0;
    bool bouncing = true;
    bool leftFacing = false;
};
SERIALIZE(FightBall2D, vertVel, horVel, gravity, floor)
COMPONENT(FightBall2D)

struct Fighter2D
{
    bool leftFacing = true;
    f32 dashCooldown = 1.0;
    f32 dashDuration = 0.25;
    f32 dashSpeed = 0.1; 
    f32 attackCooldown = 0.5;
    f32 attackDuration = 0.1;
    f32 moveSpeed = 0;
    glm::vec3 attackColor = glm::vec3{0.5};
    glm::vec3 baseColor = glm::vec3{0.25};

    std::string leftButton;
    std::string rightButton;
    std::string attackButton;
    std::string dashButton;

    bool dashing = false;
    bool attacking = false;
    f32 attackCooldownLeft = 0;
    f32 attackDurationLeft = 0;
    f32 dashCooldownLeft = 0;
    f32 dashDurationLeft = 0;
};
SERIALIZE(Fighter2D, leftFacing, dashCooldown, dashDuration, dashSpeed, attackCooldown, attackDuration, moveSpeed, attackColor, baseColor, leftButton, rightButton, attackButton, dashButton)
COMPONENT(Fighter2D)
