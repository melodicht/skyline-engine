#pragma once

#include <skl_types.h>

struct SDLMemoryBlock
{
    void *basePointer;
    SDLMemoryBlock *prev;
    SDLMemoryBlock *next;
};

struct SDLState
{
    // NOTE(marvin): Not keen on the platonic ideal of a deque with
    // inheritance of some sort, and fully encapsulating feels like
    // more trouble than it's worth at the moment.
    // The sentinel is a zeroed out memory block, also indicator of
    // the end of the deque.
    SDLMemoryBlock memoryBlockSentinel;
    TicketMutex memoryMutex;
};


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

extern SDLState globalSDLState;
