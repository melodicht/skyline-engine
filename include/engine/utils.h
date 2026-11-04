#pragma once

#include <glm/glm.hpp>

#include <game_platform.h>
#include <input.h>
#include <skl_math_types.h>

inline constexpr InputId InputMoveForward = GetInputId("MoveFwd");
inline constexpr InputId InputMoveBack = GetInputId("MoveBack");
inline constexpr InputId InputMoveLeft = GetInputId("MoveLeft");
inline constexpr InputId InputMoveRight = GetInputId("MoveRgt");
inline constexpr InputId InputLook = GetInputId("Look");

// Adds a player along with the data backing their input.
PlayerId AddInputPlayer(GameMemory &memory);

// Registers the inputs read by GetMovementDirection and looking around.
void AddMovementInputSpecs(InputSpecification &specification);
// Binds WASD and the mouse delta to the movement inputs of the given player.
void AddMovementGathering(InputSpecification &specification, PlayerId playerId);

glm::vec3 GetMovementDirection(const PlayerInput &input, Transform3D *t);
