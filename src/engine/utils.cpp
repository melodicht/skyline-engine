#include <glm/glm.hpp>

#include <utils.h>
#include <game_platform.h>
#include <skl_math_types.h>

PlayerId AddInputPlayer(GameMemory &memory)
{
    PlayerId playerId = memory.inputSpecification->AddPlayer();
    memory.playerData.emplace_back();
    memory.accumulatedPlayerData.emplace_back();
    return playerId;
}

void AddMovementInputSpecs(InputSpecification &specification)
{
    specification.AddButtonSpec(InputMoveForward);
    specification.AddButtonSpec(InputMoveBack);
    specification.AddButtonSpec(InputMoveLeft);
    specification.AddButtonSpec(InputMoveRight);
    specification.SetInput2dSpec(InputLook, {MergeBehavior2d::Add});
}

void AddMovementGathering(InputSpecification &specification, PlayerId playerId)
{
    specification.AddButtonGatheringSpec(playerId, InputMoveForward, KeyboardGather(SDLK_W));
    specification.AddButtonGatheringSpec(playerId, InputMoveBack, KeyboardGather(SDLK_S));
    specification.AddButtonGatheringSpec(playerId, InputMoveLeft, KeyboardGather(SDLK_A));
    specification.AddButtonGatheringSpec(playerId, InputMoveRight, KeyboardGather(SDLK_D));
    specification.AddInput2dGatheringSpec(playerId, InputLook, Input2dGatherType::MouseDelta);
}

glm::vec3 GetMovementDirection(const PlayerInput &input, Transform3D *t)
{
    glm::vec3 result{};

    if (input.GetButtonDown(InputMoveForward))
    {
        result += t->GetForwardVector();
    }

    if (input.GetButtonDown(InputMoveBack))
    {
        result -= t->GetForwardVector();
    }

    if (input.GetButtonDown(InputMoveRight))
    {
        result += t->GetRightVector();
    }

    if (input.GetButtonDown(InputMoveLeft))
    {
        result -= t->GetRightVector();
    }

    if (glm::length(result) > 0)
    {
        result = glm::normalize(result);
    }

    return result;
}
