#pragma once

#include <scene.h>
#include <system_registry.h>

struct Transform3D;

// TODO(marvin): Create a macro that does all the boiler plate.

class MovementSystem : public System
{
private:
    MovementSystem() {}

    MAKE_SYSTEM_DECLARATIONS(MovementSystem);
    
public:
    SYSTEM_ON_UPDATE();


private:
    // Cannot look up/down to the extent where it becomes looking behind.
    void CapVerticalRotationForward(Transform3D *t);

};
