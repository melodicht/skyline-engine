// Implements how custom memory allocation interacts with IO data
#include <platform_memory.h>


void RestoreSavedMemoryBlock(SDLSavedMemoryBlock savedMemoryBlock, SDL_IOStream* fileHandle)
{
    void* requestedBase = savedMemoryBlock.requestedBase;
    u64 requestedSize = savedMemoryBlock.requestedSize;
    siz bytesRead = SDL_ReadIO(fileHandle, requestedBase, requestedSize);
    ASSERT(bytesRead == requestedSize);
}

inline SDLSavedMemoryBlock InitSavedMemoryBlock(SDLMemoryBlock* source)
{
    SDLSavedMemoryBlock result = {};
    result.requestedBase = source->requestedBase;
    result.requestedSize = source->requestedSize;
    return result;
}

void WriteSavedMemoryBlockToFile(SDLSavedMemoryBlock* savedMemoryBlock, SDL_IOStream* fileHandle)
{
    ASSERT(SDL_WriteIO(fileHandle, savedMemoryBlock, sizeof(*savedMemoryBlock)) == sizeof(*savedMemoryBlock));
    ASSERT(SDL_WriteIO(fileHandle, savedMemoryBlock->requestedBase, savedMemoryBlock->requestedSize) == savedMemoryBlock->requestedSize);
}

void WriteMemoryBlocksToFile(SDLState* state, SDL_IOStream* fileHandle)
{
    SDLMemoryBlock* sentinel = &state->memoryBlockSentinel;
    BeginTicketMutex(&state->memoryMutex);
    for (SDLMemoryBlock* sourceBlock = sentinel->next;
         sourceBlock != sentinel;
         sourceBlock = sourceBlock->next)
    {
        SDLSavedMemoryBlock savedMemoryBlock = InitSavedMemoryBlock(sourceBlock);
        WriteSavedMemoryBlockToFile(&savedMemoryBlock, fileHandle);
    }
    EndTicketMutex(&state->memoryMutex);

    // NOTE(marvin): Ending with an empty saved block to mark the end.
    SDLSavedMemoryBlock savedMemoryBlock = {};
    WriteSavedMemoryBlockToFile(&savedMemoryBlock, fileHandle);
}

void RestoreMemoryBlocksFromFile(SDL_IOStream* fileHandle)
{
    for (;;)
    {
        SDLSavedMemoryBlock savedMemoryBlock = {};
        ASSERT(SDL_ReadIO(fileHandle, &savedMemoryBlock, sizeof(savedMemoryBlock)) == sizeof(savedMemoryBlock));
        if (savedMemoryBlock.requestedBase != 0)
        {
            RestoreSavedMemoryBlock(savedMemoryBlock, fileHandle);
        }
        else
        {
            break;
        }
    }
}
