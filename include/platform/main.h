#pragma once

#include <skl_types.h>

struct SDLMemoryBlock
{
    // NOTE(marvin): requestedBase is the base of memory that the user
    // has access to, the wholeBase is the base of the entire chunk of
    // memory, including padding at front for alignment, this memory
    // block and the user's requested memory.
    void* requestedBase;
    void* wholeBase;
    SDLMemoryBlock* prev;
    SDLMemoryBlock* next;
};

enum LoopedLiveEditingState
{
    loopedLiveEditingState_none,
    loopedLiveEditingState_recording,
    loopedLiveEditingState_playing,
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

    // NOTE(marvin): Could be in its own structure. 
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
