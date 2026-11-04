#pragma once

#include <meta_definitions.h>
#include <input.h>

struct GameState;
struct GameInput;

void FindCamera(GameState &gameState);
void DrawScene(GameState &gameState, GameInput &input, f32 deltaTime);
