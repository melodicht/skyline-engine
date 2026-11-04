#pragma once

#include <span>

#include <meta_definitions.h>
#include <input.h>

struct GameState;

void FindCamera(GameState &gameState);
void DrawScene(GameState &gameState, std::span<PlayerData> playerData, const InputSpecification &inputSpecification, f32 deltaTime, b8 profileFrame);
