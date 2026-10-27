#include <movement.h>
#include <meta_definitions.h>
#include <engine_components.h>
#include <game_components.h>
#include <utils.h>
#include <scene_view.h>

MAKE_SYSTEM_MANUAL_VTABLE(MovementSystem);

MovementSystem::MovementSystem() : SYSTEM_SUPER(MovementSystem) {}

void MovementSystem::CapVerticalRotationForward(Transform3D *transform)
{
    transform->SetLocalRotation({transform->GetLocalRotation().x, std::min(std::max(transform->GetLocalRotation().y, -90.0f), 90.0f), transform->GetLocalRotation().z});
}

SYSTEM_ON_UPDATE(MovementSystem)
{
    // TODO(marvin): Duplicate looking code between FlyingMovement and the XLook family of components.
    for (auto [entityId, flyingMovement, transform] : SceneView<FlyingMovement, Transform3D>(*scene))
    {
        transform.AddLocalRotation({0, 0, input->mouseDeltaX * flyingMovement.turnSpeed});
        transform.AddLocalRotation({0, input->mouseDeltaY * flyingMovement.turnSpeed, 0});
        this->CapVerticalRotationForward(&transform);

        glm::vec3 movementDirection = GetMovementDirection(input, &transform);
        transform.AddLocalPosition(movementDirection * flyingMovement.moveSpeed * deltaTime);
    }

    for (auto [entityId, horizontalLook, transform] : SceneView<HorizontalLook, Transform3D>(*scene))
    {
        transform.AddLocalRotation({0, 0, input->mouseDeltaX * horizontalLook.turnSpeed});
    }

    for (auto [entityId, verticalLook, transform] : SceneView<VerticalLook, Transform3D>(*scene))
    {
        transform.AddLocalRotation({0, input->mouseDeltaY * verticalLook.turnSpeed, 0});
        this->CapVerticalRotationForward(&transform);
    }
}

