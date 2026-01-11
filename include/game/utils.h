#pragma once

#include <glm/glm.hpp>

#include <game_platform.h>
#include <skl_math_types.h>

glm::vec3 GetMovementDirection(GameInput *input, Transform3D *t);