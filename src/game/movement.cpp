#include <movement.h>
#include <meta_definitions.h>
#include <engine_components.h>
#include <game_components.h>
#include <utils.h>
#include <scene_view.h>

MAKE_SYSTEM_MANUAL_VTABLE(FlyingMovementSystem);
MAKE_SYSTEM_MANUAL_VTABLE(HorizontalLookSystem);
MAKE_SYSTEM_MANUAL_VTABLE(VerticalLookSystem);

FlyingMovementSystem::FlyingMovementSystem() : SYSTEM_SUPER(FlyingMovementSystem) {}
HorizontalLookSystem::HorizontalLookSystem() : SYSTEM_SUPER(HorizontalLookSystem) {}
VerticalLookSystem::VerticalLookSystem() : SYSTEM_SUPER(VerticalLookSystem) {}

// Cannot look up/down to the extent where it becomes looking behind.
local void CapVerticalRotationForward(Transform3D *transform)
{
    transform->SetLocalRotation({transform->GetLocalRotation().x, std::min(std::max(transform->GetLocalRotation().y, -90.0f), 90.0f), transform->GetLocalRotation().z});
}

// TODO(marvin): Duplicate looking code between FlyingMovement and the XLook family of components.
COMPONENTS_ON_UPDATE_MEM(FlyingMovementSystem, const PlayerInput &input, FlyingMovement &flyingMovement, Transform3D &transform, f32 deltaTime)
{
    glm::vec2 look = input.Get2dInput(InputLook);
    transform.AddLocalRotation({0, 0, look.x * flyingMovement.turnSpeed});
    transform.AddLocalRotation({0, look.y * flyingMovement.turnSpeed, 0});
    CapVerticalRotationForward(&transform);

    glm::vec3 movementDirection = GetMovementDirection(input, &transform);
    transform.AddLocalPosition(movementDirection * flyingMovement.moveSpeed * deltaTime);
}

COMPONENTS_ON_UPDATE_MEM(HorizontalLookSystem, const PlayerInput &input, HorizontalLook &horizontalLook, Transform3D &transform)
{
    transform.AddLocalRotation({0, 0, input.Get2dInput(InputLook).x * horizontalLook.turnSpeed});
}

COMPONENTS_ON_UPDATE_MEM(VerticalLookSystem, const PlayerInput &input, VerticalLook &verticalLook, Transform3D &transform)
{
    transform.AddLocalRotation({0, input.Get2dInput(InputLook).y * verticalLook.turnSpeed, 0});
    CapVerticalRotationForward(&transform);
}
