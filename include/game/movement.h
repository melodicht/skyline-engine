#pragma once

#include <scene.h>

struct Transform3D;

class MovementSystem : public System
{
private:
    // Cannot look up/down to the extent where it becomes looking behind.
    void CapVerticalRotationForward(Transform3D *t);
public:
    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime);
};