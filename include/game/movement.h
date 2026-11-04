#pragma once

#include <scene.h>
#include <system_registry.h>
#include <engine_components.h>
#include <game_components.h>

class FlyingMovementSystem : public System
{
public:
    FlyingMovementSystem();

    COMPONENTS_ON_UPDATE(const PlayerInput &input, FlyingMovement &flyingMovement, Transform3D &transform, f32 deltaTime);
};

class HorizontalLookSystem : public System
{
public:
    HorizontalLookSystem();

    COMPONENTS_ON_UPDATE(const PlayerInput &input, HorizontalLook &horizontalLook, Transform3D &transform);
};

class VerticalLookSystem : public System
{
public:
    VerticalLookSystem();

    COMPONENTS_ON_UPDATE(const PlayerInput &input, VerticalLook &verticalLook, Transform3D &transform);
};
