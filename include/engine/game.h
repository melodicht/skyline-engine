#pragma once

#include <string>

#include <engine.h>

void OnGameStart(GameState* gameState, GameMemory* gameMemory);

void OnGameLoad(GameMemory* gameMemory);

void OnGameGetPersistentDLLPaths(const char** pathBuffer);

void OnEditorStart(GameState* gameState);
