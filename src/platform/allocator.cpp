// Responsible for the platform's memory allocator functionality,
// which allows for the game to make allocations after game
// initialize.

#include <SDL3/SDL.h>

#include <meta_definitions.h>
#include <skl_types.h>
#include <main.h>

// TODO(marvin): Clients of this interface don't go through memory.h. A great divide between treatment of fix-sized and dynamic memory in the codebase.

// TODO(marvin): In game release, if fail to allocate memory, perhaps should log the problem?

// NOTE(marvin): Each of these prefixes our memory header in front of
// the base pointer into the memory which the user may use. Every
// allocation made here is kept track in the platform's dynamic arena.

void AddMemoryBlock(SDLState *state, SDLMemoryBlock *block)
{
    SDLMemoryBlock *sentinel = &state->memoryBlockSentinel;
    
    block->next = sentinel;

    BeginTicketMutex(&state->memoryMutex);
    SDLMemoryBlock *last = sentinel->prev;
    block->prev = last;
    last->next = block;
    sentinel->prev = block;
    EndTicketMutex(&state->memoryMutex);
}

void RemoveMemoryBlock(SDLState *state, SDLMemoryBlock *block)
{
    block->prev = {};
    block->next = {};

    BeginTicketMutex(&state->memoryMutex);
    SDLMemoryBlock *prev = block->prev;
    SDLMemoryBlock *next = block->next;
    prev->next = next;
    next->prev = prev;
    EndTicketMutex(&state->memoryMutex);
}


inline siz RoundUpToMultiple(siz value, siz N) {
    siz result = ((value + N - 1) / N) * N;
    return result;
}

void *AlignedAllocate(siz requestedSize, siz alignment)
{
    // NOTE(marvin): As to not mess with the alignment. There might be
    // space at the front that has to be sacrificed.
    siz sizeForMemoryBlock = RoundUpToMultiple(sizeof(SDLMemoryBlock), alignment);
    siz totalSize = sizeForMemoryBlock + requestedSize;
    void *base = SDL_aligned_alloc(alignment, totalSize);

#if SKL_SLOW
    if(base == NULL)
    {
        LOG_ERROR(SDL_GetError());
        Assert(false && "Failed to aligned allocate.");
    }
#endif

    u8 *requestedBase = static_cast<u8 *>(base) + sizeForMemoryBlock;
    SDLMemoryBlock *memoryBlockBase = reinterpret_cast<SDLMemoryBlock *>(requestedBase - sizeof(SDLMemoryBlock));
    void *result = static_cast<void *>(requestedBase);
    memoryBlockBase->basePointer = result;
    AddMemoryBlock(&globalSDLState, memoryBlockBase);

    return result;
}

void AlignedFree(void *block)
{
    SDLMemoryBlock *memoryBlockBase = static_cast<SDLMemoryBlock *>(block) - 1;
    RemoveMemoryBlock(&globalSDLState, memoryBlockBase);
    SDL_aligned_free(memoryBlockBase);
}

// TODO(marvin): The non-aligned de/allocation procedures look very similar to their aligned counterparts. Main difference is round up to multiple and figuring out where memory block base is. Is it worth abstracting? 

void *Allocate(siz requestedSize)
{
    siz sizeForMemoryBlock = sizeof(SDLMemoryBlock);
    siz totalSize = sizeForMemoryBlock + requestedSize;
    void *base = SDL_malloc(totalSize);

#if SKL_SLOW
    if(base == NULL)
    {
        LOG_ERROR(SDL_GetError());
        Assert(false && "Failed to allocate.");
    }
#endif

    u8 *requestedBase = static_cast<u8 *>(base) + sizeForMemoryBlock;
    SDLMemoryBlock *memoryBlockBase = static_cast<SDLMemoryBlock *>(base);
    void *result = static_cast<void *>(requestedBase);
    memoryBlockBase->basePointer = result;
    AddMemoryBlock(&globalSDLState, memoryBlockBase);
    return result;
}

void Free(void *block)
{
    // TODO(marvin): Very similar to AlignedFree, except SDL_free instead of SDL_aligned_free.... Is it worth abstracting?
    SDLMemoryBlock *memoryBlockBase = static_cast<SDLMemoryBlock *>(block) - 1;
    RemoveMemoryBlock(&globalSDLState, memoryBlockBase);
    SDL_free(memoryBlockBase);
}

void *Realloc(void *block, siz oldRequestedSize, siz newRequestedSize)
{
    siz sizeForMemoryBlock = sizeof(SDLMemoryBlock);
    siz newTotalSize = sizeForMemoryBlock + newRequestedSize;

    SDLMemoryBlock *oldMemoryBlockBase = static_cast<SDLMemoryBlock *>(block) - 1;
    void *newBase = SDL_realloc(oldMemoryBlockBase, newTotalSize);
    SDLMemoryBlock *newMemoryBlockBase = static_cast<SDLMemoryBlock *>(newBase);
    u8 *requestedBase = static_cast<u8 *>(newBase) + sizeForMemoryBlock;
    void *result = static_cast<void *>(requestedBase);
    
    if (newMemoryBlockBase != oldMemoryBlockBase)
    {
        newMemoryBlockBase->basePointer = result;
        RemoveMemoryBlock(&globalSDLState, oldMemoryBlockBase);
        AddMemoryBlock(&globalSDLState, newMemoryBlockBase);
    }

    return result;
}
