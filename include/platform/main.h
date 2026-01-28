#pragma once

#include <skl_types.h>

enum SDLMemoryFlags
{
    sdlMem_none = 0b0,
    sdlMem_allocatedDuringLoop = 0b1,
    sdlMem_freedDuringLoop = 0b10,
};

struct SDLMemoryBlock
{
    // NOTE(marvin): requestedBase is the base of memory that the user
    // has access to, the wholeBase is the base of the entire chunk of
    // memory, including padding at front for alignment, this memory
    // block and the user's requested memory.
    void* requestedBase;
    u64 requestedSize;
    void* wholeBase;
    SDLMemoryBlock* prev;
    SDLMemoryBlock* next;
    SDLMemoryFlags loopingFlags;
};

struct SDLSavedMemoryBlock
{
    void* requestedBase;
    u64 requestedSize;
};

enum LoopedLiveEditingState
{
    loopedLiveEditingState_none,
    loopedLiveEditingState_recording,
    loopedLiveEditingState_playing,
};

struct SDLState
{
    // TODO(marvin): Could be in its own structure.
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

    // NOTE(marvin): nullptr if there is no game process.
    SDL_Process* gameProcess;
};

inline b32 SDLIsInLoop(SDLState* state)
{
    b32 result = (state->loopedLiveEditingState != loopedLiveEditingState_none);
    return result;
}


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
