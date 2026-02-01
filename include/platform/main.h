#pragma once

#include <platform_memory.h>
#include <skl_types.h>

inline void InitMemoryBlockDeque(SDLMemoryBlock *sentinel)
{
    sentinel->prev = sentinel;
    sentinel->next = sentinel;
}

inline void InitSDLState(SDLState *state)
{
    InitMemoryBlockDeque(&state->memoryBlockSentinel);
    state->memoryMutex = {};
}
