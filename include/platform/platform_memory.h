#pragma once

#include <SDL3/SDL.h>

#include <meta_definitions.h>
#include <skl_types.h>
#include <platform_loop.h>

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
    
    // Ensures that only loop utils will be able to access looping flags
#if SKL_INTERNAL
    friend class LoopUtils;
private:
    LoopMemoryFlags loopingFlags;
#endif
};

struct SDLSavedMemoryBlock
{
    void* requestedBase;
    u64 requestedSize;
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

    // NOTE(marvin): nullptr if there is no game process.
#if !EMSCRIPTEN
    SDL_Process* gameProcess;
#endif

    // Ensures that only loop utils will be able to access loop state
#if SKL_INTERNAL
    friend class LoopUtils;
private:
    LoopState loopState;
#endif
};

extern SDLState globalSDLState;

// >>> Platform Allocator helpers <<<
void AddMemoryBlock(SDLState *state, SDLMemoryBlock *block);

void RemoveMemoryBlock(SDLState *state, SDLMemoryBlock *block);

// >>> Memory / IO interop logic
void RestoreSavedMemoryBlock(SDLSavedMemoryBlock savedMemoryBlock, SDL_IOStream* fileHandle);

inline SDLSavedMemoryBlock InitSavedMemoryBlock(SDLMemoryBlock* source);

void WriteSavedMemoryBlockToFile(SDLSavedMemoryBlock* savedMemoryBlock, SDL_IOStream* fileHandle);

void WriteMemoryBlocksToFile(SDLState* state, SDL_IOStream* fileHandle);

void RestoreMemoryBlocksFromFile(SDL_IOStream* fileHandle);