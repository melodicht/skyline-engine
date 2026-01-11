#include <glm/glm.hpp>

#include <utils.h>
#include <game_platform.h>
#include <skl_math_types.h>

glm::vec3 GetMovementDirection(GameInput *input, Transform3D *t)
{
    glm::vec3 result{};

    if (input->keysDown.contains("W"))
    {
        result += t->GetForwardVector();
    }

    if (input->keysDown.contains("S"))
    {
        result -= t->GetForwardVector();
    }

    if (input->keysDown.contains("D"))
    {
        result += t->GetRightVector();
    }

    if (input->keysDown.contains("A"))
    {
        result -= t->GetRightVector();
    }

    return result;
}
