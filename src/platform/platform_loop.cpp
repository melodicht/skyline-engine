#include <platform_loop.h>

#if SKL_INTERNAL
#include <algorithm>

#include <platform_memory.h>
#include <game_platform.h>

// >>> Local Helper Functions <<<
// Upper bound of a recorded frame: each player's input.
local siz SDLGetFrameBufferCapacity(siz playerCount)
{
    return playerCount * MaxBufferSize;
}

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

void LoopUtils::SDLBeginInputPlayback(SDLState* state, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData)
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
        *accumulatedPlayerData = state->loopState.recordedAccumulatedPlayerData;
        // Recorded frames don't hold previous button state, so the first frame gets it from here.
        ASSERT_PRINT(playerData.size() == state->loopState.recordedPlayerData.size(), "Player count changed during looped-live editing.");
        std::copy(state->loopState.recordedPlayerData.begin(), state->loopState.recordedPlayerData.end(), playerData.begin());
    }
}

void LoopUtils::SDLEndInputPlayback(SDLState* state)
{
    SDLClearBlocksByMask(state, LoopMemoryFlags::sdlMem_freedDuringLoop);
    
    TRY(SDL_CloseIO(state->loopState.playbackHandle));
    state->loopState.loopedLiveEditingState = LoopState::LoopedLiveEditingState::none;
}

void LoopUtils::SDLBeginRecordingInput(SDLState* state, std::span<const PlayerData> playerData, const std::vector<PlayerData>* accumulatedPlayerData)
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
        state->loopState.recordedPlayerData.assign(playerData.begin(), playerData.end());
        state->loopState.recordedAccumulatedPlayerData = *accumulatedPlayerData;
        WriteMemoryBlocksToFile(state, state->loopState.recordingHandle);
    }
    
}

void LoopUtils::SDLEndRecordingInput(SDLState* state)
{
    TRY(SDL_CloseIO(state->loopState.recordingHandle));
    state->loopState.loopedLiveEditingState = LoopState::LoopedLiveEditingState::none;
}

void LoopUtils::SDLRecordInput(SDLState* state, const InputSpecification& specification, std::span<const PlayerData> playerData)
{
    std::vector<std::byte>& buffer = state->loopState.frameBuffer;
    buffer.resize(SDLGetFrameBufferCapacity(playerData.size()));

    std::byte* cursor = buffer.data();
    for (const PlayerData& data : playerData)
    {
        cursor += specification.WriteBuffer(cursor, data);
    }

    u32 frameSize = static_cast<u32>(cursor - buffer.data());
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, &frameSize, sizeof(frameSize)), sizeof(frameSize));
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, buffer.data(), frameSize), frameSize);
}

// Did a reset happen?
b8 LoopUtils::SDLPlaybackInput(SDLState* state, const InputSpecification& specification, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData, b8 forceReloadGameCode)
{
    // NOTE(marvin): Would it be possible for there to be two notion
    // of mouse? One used in the game, and one for interacting with
    // the editor?

    b32 result = false;

    u32 frameSize = 0;
    siz bytesRead = SDL_ReadIO(state->loopState.playbackHandle, &frameSize, sizeof(frameSize));

    if (bytesRead == 0 || forceReloadGameCode)
    {
        // NOTE(marvin): Could also rewind the stream, but going
        // through end and start again is safer.
        SDLEndInputPlayback(state);
        SDLBeginInputPlayback(state, playerData, accumulatedPlayerData);
        bytesRead = SDL_ReadIO(state->loopState.playbackHandle, &frameSize, sizeof(frameSize));
        result = true;
    }
    ASSERT(bytesRead == sizeof(frameSize));

    std::vector<std::byte>& buffer = state->loopState.frameBuffer;
    buffer.resize(SDLGetFrameBufferCapacity(playerData.size()));
    ASSERT_PRINT(frameSize <= buffer.size(), "Recorded frame doesn't fit the current input specification.");
    TRY_EXPECT(SDL_ReadIO(state->loopState.playbackHandle, buffer.data(), frameSize), frameSize);

    const std::byte* cursor = buffer.data();
    for (PlayerData& data : playerData)
    {
        cursor += specification.ReadBuffer(cursor, data);
    }
    ASSERT_PRINT(cursor == buffer.data() + frameSize, "Recorded frame doesn't match the current input specification.");
    return result;
}

// >>> Public Interface <<<
b8 LoopUtils::GetIsStateInLoop(const SDLState* state)
{
    return state->loopState.loopedLiveEditingState != LoopState::LoopedLiveEditingState::none;
}

b8 LoopUtils::GetIsStateInPlayback(const SDLState* state)
{
    return state->loopState.loopedLiveEditingState == LoopState::LoopedLiveEditingState::playing;
}

void LoopUtils::ToggleLoopedLiveEditingState(SDLState* state, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData)
{
    switch (state->loopState.loopedLiveEditingState)
    {
      case LoopState::LoopedLiveEditingState::none:
      {
          SDLBeginRecordingInput(state, playerData, accumulatedPlayerData);
      } break;
      case LoopState::LoopedLiveEditingState::recording:
      {
          SDLEndRecordingInput(state);
          SDLBeginInputPlayback(state, playerData, accumulatedPlayerData);
      } break;
      case LoopState::LoopedLiveEditingState::playing:
      {
          SDLEndInputPlayback(state);
      } break;
    }
}

b8 LoopUtils::ProcessInputWithLooping(SDLState* state, const InputSpecification& specification, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData, b8 forceReloadGameCode)
{
    b32 result = false;
    switch (state->loopState.loopedLiveEditingState)
    {
      case LoopState::LoopedLiveEditingState::none: {} break;
      case LoopState::LoopedLiveEditingState::recording:
      {
          SDLRecordInput(state, specification, playerData);
      } break;
      case LoopState::LoopedLiveEditingState::playing:
      {
          result = SDLPlaybackInput(state, specification, playerData, accumulatedPlayerData, forceReloadGameCode);
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
void LoopUtils::ToggleLoopedLiveEditingState(SDLState* state, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData) {}
b8 LoopUtils::ProcessInputWithLooping(SDLState* state, const InputSpecification& specification, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData, b8 forceReloadGameCode) { return false; }
void LoopUtils::SetBlockFlagLoopAllocated(SDLMemoryBlock* block) {}
void LoopUtils::SetBlockFlagLoopFreed(SDLMemoryBlock* block) {}
void LoopUtils::SetBlockFlagLoopNone(SDLMemoryBlock* block) {}
b8 LoopUtils::GetBlockFlagLoopAllocated(const SDLMemoryBlock* block) { return true; }
#endif
