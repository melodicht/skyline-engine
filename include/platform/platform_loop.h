#pragma once

// This file controls how the editor controls live loop editing.
// Allows the storing of editor 

#include <SDL3/SDL.h>

#include <set>

#include <meta_definitions.h>

struct SDLState;
struct GameInput;

enum SDLMemoryFlags
{
    sdlMem_none = 0b0,
    sdlMem_allocatedDuringLoop = 0b1,
    sdlMem_freedDuringLoop = 0b10,
};

enum class LoopedLiveEditingState : u8
{
    none,
    recording,
    playing,
};

struct LoopState {
    LoopedLiveEditingState loopedLiveEditingState;
    union
    {
        // NOTE(marvin): A flat sequence of [GameInput, u32 (for
        // number of elements in keysDown set), a sequence of [u32
        // (for number of elements in string), characters that make
        // the string]]
        SDL_IOStream* recordingHandle;
        SDL_IOStream* playbackHandle;
    };
};

bool SDLIsInLoop(SDLState* state);

void ToggleLoopedLiveEditingState(SDLState* state);

// Based on live editing state, stores or plays game input
void ProcessInputWithLooping(SDLState* state, GameInput* gameInput);

// Returns whether in loop
bool SetFlagAllocatedIfInLoop(SDLState* state, SDLMemoryFlags* flag);
bool SetFlagFreedIfInLoop(SDLState* state, SDLMemoryFlags* flag);
