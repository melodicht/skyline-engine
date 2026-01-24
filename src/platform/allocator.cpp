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

    last->loopingFlags = static_cast<SDLMemoryFlags>(SDLIsInLoop(state) ? sdlMem_allocatedDuringLoop : 0);
}

void RemoveMemoryBlock(SDLState *state, SDLMemoryBlock *block)
{
    if (SDLIsInLoop(state))
    {
        block->loopingFlags = sdlMem_freedDuringLoop;
    }
    else
    {
        BeginTicketMutex(&state->memoryMutex);
        SDLMemoryBlock *prev = block->prev;
        SDLMemoryBlock *next = block->next;
        prev->next = next;
        next->prev = prev;
        EndTicketMutex(&state->memoryMutex);

        block->prev = {};
        block->next = {};
    }
}


inline siz RoundUpToMultiple(siz value, siz N) {
    siz result = ((value + N - 1) / N) * N;
    return result;
}

void* AlignedAllocate(siz requestedSize, siz alignment)
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
    ASSERT(IsAligned(requestedBase, alignment));
    SDLMemoryBlock *memoryBlockBase = reinterpret_cast<SDLMemoryBlock *>(requestedBase - sizeof(SDLMemoryBlock));
    void *result = static_cast<void *>(requestedBase);
    memoryBlockBase->requestedBase = result;
    memoryBlockBase->wholeBase = base;
    memoryBlockBase->requestedSize = requestedSize;
    AddMemoryBlock(&globalSDLState, memoryBlockBase);

    return result;
}

void AlignedFree(void* block)
{
    SDLMemoryBlock *memoryBlockBase = static_cast<SDLMemoryBlock*>(block) - 1;
    void* toFree = memoryBlockBase->wholeBase;
    RemoveMemoryBlock(&globalSDLState, memoryBlockBase);
    SDL_aligned_free(toFree);
}

// TODO(marvin): The non-aligned de/allocation procedures look very similar to their aligned counterparts. Main difference is round up to multiple and figuring out where memory block base is. Is it worth abstracting? 

void* Allocate(siz requestedSize)
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
    memoryBlockBase->requestedBase = result;
    memoryBlockBase->wholeBase = base;
    memoryBlockBase->requestedSize = requestedSize;
    AddMemoryBlock(&globalSDLState, memoryBlockBase);
    return result;
}

void Free(void* block)
{
    // TODO(marvin): Very similar to AlignedFree, except SDL_free instead of SDL_aligned_free.... Is it worth abstracting?
    SDLMemoryBlock* memoryBlockBase = static_cast<SDLMemoryBlock*>(block) - 1;
    void* toFree = memoryBlockBase->wholeBase;
    RemoveMemoryBlock(&globalSDLState, memoryBlockBase);
    SDL_free(toFree);
}

void* Realloc(void* block, siz oldRequestedSize, siz newRequestedSize)
{
    if (block == nullptr)
    {
        ASSERT(oldRequestedSize == 0);
        return Allocate(newRequestedSize);
    }
    
    siz sizeForMemoryBlock = sizeof(SDLMemoryBlock);
    siz newTotalSize = sizeForMemoryBlock + newRequestedSize;

    // NOTE(marvin): Need to preserve padding if any.

    SDLMemoryBlock* oldMemoryBlockBase = static_cast<SDLMemoryBlock*>(block) - 1;
    void* oldMemoryBlockBaseAddr = static_cast<void*>(oldMemoryBlockBase);
    void* oldWholeBase = oldMemoryBlockBase->wholeBase;
    siz padding = static_cast<u8*>(oldMemoryBlockBaseAddr) - static_cast<u8*>(oldWholeBase);
    
    void* newBase = SDL_realloc(oldWholeBase, newTotalSize);
    void* newMemoryBlockBaseAddr = static_cast<void*>(static_cast<u8*>(newBase) + padding);
    SDLMemoryBlock* newMemoryBlockBase = static_cast<SDLMemoryBlock*>(newMemoryBlockBaseAddr);
    u8* requestedBase = static_cast<u8*>(newMemoryBlockBaseAddr) + sizeForMemoryBlock;
    void* result = static_cast<void*>(requestedBase);
    
    if (newMemoryBlockBase != oldMemoryBlockBase)
    {
        newMemoryBlockBase->requestedBase = result;
        newMemoryBlockBase->wholeBase = newBase;
        newMemoryBlockBase->requestedSize = newRequestedSize;
        RemoveMemoryBlock(&globalSDLState, oldMemoryBlockBase);
        AddMemoryBlock(&globalSDLState, newMemoryBlockBase);
    }

    return result;
}


