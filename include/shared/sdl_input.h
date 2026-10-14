#pragma once

#include <SDL.h>

#include "input.h"

InputSpecification::PlayerInput GatherPollingSDL(const InputSpecification& specification, const SDL_Event& e, const InputSpecification::PlayerInput& prevInput);