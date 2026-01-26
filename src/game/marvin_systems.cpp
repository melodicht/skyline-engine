#include <marvin_systems.h>
#include <game.h>
#include <scene.h>
#include <meta_definitions.h>
#include <components.h>
#include <scene_view.h>

MAKE_SYSTEM_MANUAL_VTABLE(GravityBallsSystem);

GravityBallsSystem::GravityBallsSystem() : SYSTEM_SUPER(GravityBallsSystem) {
    this->triggerWasDown = false;
}

SYSTEM_ON_UPDATE(GravityBallsSystem)
{
    // NOTE(marvin): When player right clicks, spawns a gravity ball, heading the
    // direction that the user is facing.
    b32 triggerIsDown = input->keysDown.contains("Mouse 3");

    // TODO(marvin): The idea of "on press" should be encoded into the input struct.
    if (triggerIsDown and !triggerWasDown)
    {
        EntityID gravityBall = scene->NewEntity();
        GravityBall *gb = scene->Assign<GravityBall>(gravityBall);
        MeshComponent *m = scene->Assign<MeshComponent>(gravityBall);
        m->mesh = assetUtils.LoadMeshAsset("cube");

        Transform3D *t = scene->Assign<Transform3D>(gravityBall);

        // NOTE(marvin): Get the camera
        SceneView<CameraComponent, Transform3D> cameraView = SceneView<CameraComponent, Transform3D>(*scene);
        if (cameraView.begin() == cameraView.end())
        {
            return;
        }
        EntityID cameraEnt = *cameraView.begin();
        Transform3D *ct = scene->Get<Transform3D>(cameraEnt);
        glm::vec3 cameraPosition = ct->GetWorldPosition();
        glm::vec3 cameraRotation = ct->GetForwardVector();
        t->SetLocalPosition(cameraPosition);
        t->SetLocalRotation(cameraRotation);
    }

    this->triggerWasDown = triggerIsDown;

    // NOTE(marvin): Gravity ball gradually grows in size. Maybe gravity ball has a time alive variable which size and possibly speed can use?
    for (EntityID ent : SceneView<GravityBall, Transform3D>(*scene))
    {
        GravityBall* gb = scene->Get<GravityBall>(ent);
        Transform3D* t = scene->Get<Transform3D>(ent);
        if (!gb->activated)
        {
            f32 lifetime = gb->lifetime;
            // NOTE(marvin): At t=0, multiplier is 1. As t approaches
            // infinity, multipler approaches 2.
            f32 sharpness = -0.02;
            f32 sizeMultiplier = 10.0f - 9.0f * std::exp(sharpness * lifetime);
            // NOTE(marvin): Assumes that the transform has an original scale of 1.
            glm::vec3 newScale{sizeMultiplier};
            t->SetLocalScale(newScale);
        }
    }

}
