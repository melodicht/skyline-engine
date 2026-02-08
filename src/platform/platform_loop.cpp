#include <platform_loop.h>

#if SKL_INTERNAL
#include <platform_memory.h>
#include <game_platform.h>

// >>> Local Helper Functions <<<
const char* LoopUtils::SDLGetInputFilePath()
{
    const char* result = "loop_start.skli";
    return result;
}

void LoopUtils::SDLClearBlocksByMask(SDLState* state, LoopMemoryFlags::SDLMemoryFlags mask)
{
    SDLMemoryBlock* sentinel = &state->memoryBlockSentinel;
    // NOTE(marvin): Need to set the next prior to removing the block.
    for (SDLMemoryBlock* cursor = sentinel->next;
         cursor != sentinel;
         )
    {
        SDLMemoryBlock* block = cursor;
        cursor = cursor->next;
        
        if ((block->loopingFlags.m_flags & mask) == mask)
        {
            RemoveMemoryBlock(state, block);
        }
        else
        {
            block->loopingFlags.m_flags = LoopMemoryFlags::sdlMem_none;
        }
    }
}

void LoopUtils::SDLBeginInputPlayback(SDLState* state)
{
    SDLClearBlocksByMask(state, LoopMemoryFlags::sdlMem_allocatedDuringLoop);
    
    const char* inputFilePath = SDLGetInputFilePath();
    state->loopState.playbackHandle = SDL_IOFromFile(inputFilePath, "r");
    if (state->loopState.playbackHandle == NULL)
    {
        LOG_ERROR(SDL_GetError());
        ASSERT_PRINT(false, "Failed to read recorded input file for looped-live editing.");
    }
    else
    {
        state->loopState.loopedLiveEditingState = LoopState::LoopedLiveEditingState::playing;
        RestoreMemoryBlocksFromFile(state->loopState.playbackHandle);
    }
}

void LoopUtils::SDLEndInputPlayback(SDLState* state)
{
    SDLClearBlocksByMask(state, LoopMemoryFlags::sdlMem_freedDuringLoop);
    
    TRY(SDL_CloseIO(state->loopState.playbackHandle));
    state->loopState.loopedLiveEditingState = LoopState::LoopedLiveEditingState::none;
}

void LoopUtils::SDLBeginRecordingInput(SDLState* state)
{
    const char* inputFilePath = SDLGetInputFilePath();
    state->loopState.recordingHandle = SDL_IOFromFile(inputFilePath, "w");
    if (state->loopState.recordingHandle == NULL)
    {
        LOG_ERROR(SDL_GetError());
        ASSERT_PRINT(false, "Failed to create recording handle for looped-live editing.");
    }
    else
    {
        state->loopState.loopedLiveEditingState = LoopState::LoopedLiveEditingState::recording;
        WriteMemoryBlocksToFile(state, state->loopState.recordingHandle);
    }
    
}

void LoopUtils::SDLEndRecordingInput(SDLState* state)
{
    TRY(SDL_CloseIO(state->loopState.recordingHandle));
    state->loopState.loopedLiveEditingState = LoopState::LoopedLiveEditingState::none;
}

void LoopUtils::SDLRecordStdString(SDLState* state, const std::string* str)
{
    u32 length = static_cast<u32>(str->size());
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, &length, sizeof(length)), sizeof(length));
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, str->data(), length), length);
}

void LoopUtils::SDLRecordStdSetOfString(SDLState* state, const std::set<std::string>* strings)
{
    u32 count = static_cast<u32>(strings->size());
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, &count, sizeof(count)), sizeof(count));

    for (const std::string& str : *strings)
    {
        SDLRecordStdString(state, &str);
    }
}

void LoopUtils::SDLRecordInput(SDLState* state, const GameInput* gameInput)
{
    // NOTE(marvin): The keys down is not going to be used.
    siz bytesWritten = SDL_WriteIO(state->loopState.recordingHandle, gameInput, sizeof(*gameInput));
    ASSERT_PRINT(bytesWritten == sizeof(*gameInput), "Failed to properly write.");

    // NOTE(marvin): Have to manually deserialize std set.
    SDLRecordStdSetOfString(state, &gameInput->keysDownPrevFrame);
    SDLRecordStdSetOfString(state, &gameInput->keysDownThisFrame);
}

void LoopUtils::SDLPlaybackStdString(SDLState* state, std::set<std::string>* strings)
{
    u32 length;
    TRY_EXPECT(SDL_ReadIO(state->loopState.playbackHandle, &length, sizeof(length)), sizeof(length));

    std::string str(length, '\0');
    TRY_EXPECT(SDL_ReadIO(state->loopState.playbackHandle, str.data(), length), length);
    strings->insert(std::move(str));
}

void LoopUtils::SDLPlaybackStdSetOfString(SDLState* state, std::set<std::string>* strings)
{
    u32 count;
    TRY_EXPECT(SDL_ReadIO(state->loopState.playbackHandle, &count, sizeof(count)), sizeof(count));

    for (u32 index = 0; index < count; ++index)
    {
        SDLPlaybackStdString(state, strings);
    }
}

// Did a reset happen?
b8 LoopUtils::SDLPlaybackInput(SDLState* state, GameInput* gameInput)
{
    // NOTE(marvin): Would it be possible for there to be two notion
    // of mouse? One used in the game, and one for interacting with
    // the editor?

    b32 result = false;

    // NOTE(marvin): Need to destroy the std set prior to writing to
    // it because the write will corrupt std set memory. Reconstruct it after.
    gameInput->keysDownPrevFrame.~set();
    gameInput->keysDownThisFrame.~set();
    siz bytesRead = SDL_ReadIO(state->loopState.playbackHandle, gameInput, sizeof(*gameInput));

    if (bytesRead == 0)
    {
        // NOTE(marvin): Could also rewind the stream, but going
        // through end and start again is safer.
        SDLEndInputPlayback(state);
        SDLBeginInputPlayback(state);
        bytesRead = SDL_ReadIO(state->loopState.playbackHandle, gameInput, sizeof(*gameInput));
        result = true;
    }
    ASSERT(bytesRead == sizeof(*gameInput));
    
    new (&gameInput->keysDownPrevFrame) std::set<std::string>();
    new (&gameInput->keysDownThisFrame) std::set<std::string>();
    SDLPlaybackStdSetOfString(state, &gameInput->keysDownPrevFrame);
    SDLPlaybackStdSetOfString(state, &gameInput->keysDownThisFrame);
    return result;
}

// >>> Public Interface <<<
b8 LoopUtils::GetIsStateInLoop(const SDLState* state)
{
    return state->loopState.loopedLiveEditingState != LoopState::LoopedLiveEditingState::none;
}

void LoopUtils::ToggleLoopedLiveEditingState(SDLState* state)
{
    switch (state->loopState.loopedLiveEditingState)
    {
      case LoopState::LoopedLiveEditingState::none:
      {
          SDLBeginRecordingInput(state);
      } break;
      case LoopState::LoopedLiveEditingState::recording:
      {
          SDLEndRecordingInput(state);
          SDLBeginInputPlayback(state);
      } break;
      case LoopState::LoopedLiveEditingState::playing:
      {
          SDLEndInputPlayback(state);
      } break;
    }
}

b8 LoopUtils::ProcessInputWithLooping(SDLState* state, GameInput* gameInput)
{
    b32 result = false;
    switch (state->loopState.loopedLiveEditingState)
    {
      case LoopState::LoopedLiveEditingState::none: {} break;
      case LoopState::LoopedLiveEditingState::recording:
      {
          SDLRecordInput(state, gameInput);
      } break;
      case LoopState::LoopedLiveEditingState::playing:
      {
          result = SDLPlaybackInput(state, gameInput);
      } break;
    }
    return result;
}

void LoopUtils::SetBlockFlagLoopAllocated(SDLMemoryBlock* block) {
    block->loopingFlags.m_flags = LoopMemoryFlags::sdlMem_allocatedDuringLoop;
}
void LoopUtils::SetBlockFlagLoopFreed(SDLMemoryBlock* block) {
    block->loopingFlags.m_flags = LoopMemoryFlags::sdlMem_freedDuringLoop;
}

void LoopUtils::SetBlockFlagLoopNone(SDLMemoryBlock* block) {
    block->loopingFlags.m_flags = LoopMemoryFlags::sdlMem_freedDuringLoop;
}

b8 LoopUtils::GetBlockFlagLoopAllocated(const SDLMemoryBlock* block)  {
    return block->loopingFlags.m_flags == LoopMemoryFlags::sdlMem_allocatedDuringLoop;
}
#else 
b8 LoopUtils::GetIsStateInLoop(const SDLState* state) {}
void LoopUtils::ToggleLoopedLiveEditingState(SDLState* state) {}
b8 LoopUtils::ProcessInputWithLooping(SDLState* state, GameInput* gameInput) { return false; }
void LoopUtils::SetBlockFlagLoopAllocated(SDLMemoryBlock* block) {}
void LoopUtils::SetBlockFlagLoopFreed(SDLMemoryBlock* block) {}
void LoopUtils::SetBlockFlagLoopNone(SDLMemoryBlock* block) {}
b8 LoopUtils::GetBlockFlagLoopAllocated(const SDLMemoryBlock* block) { return true; }
#endif
