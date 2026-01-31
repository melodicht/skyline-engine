#include <glm/glm.hpp>

#include <utils.h>
#include <game_platform.h>
#include <skl_math_types.h>

glm::vec3 GetMovementDirection(GameInput *input, Transform3D *t)
{
    glm::vec3 result{};

    if (OnHold(input, "W"))
    {
        result += t->GetForwardVector();
    }

    if (OnHold(input, "S"))
    {
        result -= t->GetForwardVector();
    }

    if (OnHold(input, "D"))
    {
        result += t->GetRightVector();
    }

    if (OnHold(input, "A"))
    {
        result -= t->GetRightVector();
    }

    if (glm::length(result) > 0)
    {
        result = glm::normalize(result);
    }

    return result;
}
