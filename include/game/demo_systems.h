#pragma once

#include <scene.h>
#include <system_registry.h>

struct Transform3D;

class RectCollision2DSystem : public System
{
public:
    RectCollision2DSystem();

    SYSTEM_ON_UPDATE();

};

class PongMovement2DSystem : public System
{
public:
    PongMovement2DSystem();

    SYSTEM_ON_UPDATE();

};

class PongBall2DSystem : public System
{
public:
    PongBall2DSystem();

    SYSTEM_ON_UPDATE();

};
