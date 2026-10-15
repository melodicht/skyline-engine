#pragma once

#include <SDL.h>

#include "input.h"

void GatherPollingSDL(InputSpecification& specification, const SDL_Event& e, std::span<PlayerInput> inputs);
