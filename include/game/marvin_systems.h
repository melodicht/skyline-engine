#pragma once

#include <scene.h>
#include <system_registry.h>

class GravityBallsSystem : public System
{
public:
    GravityBallsSystem();
    
    SYSTEM_ON_UPDATE();
private:
    b32 triggerWasDown;
};
