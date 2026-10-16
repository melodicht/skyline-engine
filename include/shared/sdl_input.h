#pragma once

#include <SDL.h>

#include "input.h"

// Streams in relevant inputs from each event
void GatherInputsFromEvent(const InputSpecification& specification, const SDL_Event& e, std::span<PlayerData> inputs);
// Gets all non-event specific inputs
void GatherInputs(const InputSpecification& specification, std::span<PlayerData> inputs);
