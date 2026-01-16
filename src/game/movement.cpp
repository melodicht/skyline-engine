#include <movement.h>
#include <meta_definitions.h>
#include <components.h>
#include <utils.h>
#include <scene_view.h>

void MovementSystem::CapVerticalRotationForward(Transform3D *t)
{
    t->SetLocalRotation({t->GetLocalRotation().x, std::min(std::max(t->GetLocalRotation().y, -90.0f), 90.0f), t->GetLocalRotation().z});
}

void MovementSystem::OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
{
    // TODO(marvin): Duplicate looking code between FlyingMovement and the XLook family of components.
    for (EntityID ent: SceneView<FlyingMovement, Transform3D>(*scene))
    {
        FlyingMovement *f = scene->Get<FlyingMovement>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);

        t->AddLocalRotation({0, 0, input->mouseDeltaX * f->turnSpeed});
        t->AddLocalRotation({0, input->mouseDeltaY * f->turnSpeed, 0});
        this->CapVerticalRotationForward(t);

        glm::vec3 movementDirection = GetMovementDirection(input, t);
        t->AddLocalPosition(movementDirection * f->moveSpeed * deltaTime);
    }

    for (EntityID ent : SceneView<HorizontalLook, Transform3D>(*scene))
    {
        HorizontalLook *hl = scene->Get<HorizontalLook>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);
        t->AddLocalRotation({0, 0, input->mouseDeltaX * hl->turnSpeed});
    }

    for (EntityID ent : SceneView<VerticalLook, Transform3D>(*scene))
    {
        VerticalLook *vl = scene->Get<VerticalLook>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);
        t->AddLocalRotation({0, input->mouseDeltaY * vl->turnSpeed, 0});
        this->CapVerticalRotationForward(t);
    }
}