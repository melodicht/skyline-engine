#include <platform_loop.h>

// >>> Local Helper Functions <<<
#if SKL_INTERNAL
#include <platform_memory.h>
#include <game_platform.h>
local const char* SDLGetInputFilePath()
{
    const char* result = "loop_start.skli";
    return result;
}

bool SDLIsInLoop(SDLState* state)
{
    return state->loopState.loopedLiveEditingState != LoopedLiveEditingState::none;
}

local void SDLClearBlocksByMask(SDLState* state, SDLMemoryFlags mask)
{
    SDLMemoryBlock* sentinel = &state->memoryBlockSentinel;
    // NOTE(marvin): Need to set the next prior to removing the block.
    for (SDLMemoryBlock* cursor = sentinel->next;
         cursor != sentinel;
         )
    {
        SDLMemoryBlock* block = cursor;
        cursor = cursor->next;
        
        if ((block->loopingFlags & mask) == mask)
        {
            RemoveMemoryBlock(state, block);
        }
        else
        {
            block->loopingFlags = sdlMem_none;
        }
    }
}

local void SDLBeginInputPlayback(SDLState* state)
{
    SDLClearBlocksByMask(state, sdlMem_allocatedDuringLoop);
    
    const char* inputFilePath = SDLGetInputFilePath();
    state->loopState.playbackHandle = SDL_IOFromFile(inputFilePath, "r");
    if (state->loopState.playbackHandle == NULL)
    {
        LOG_ERROR(SDL_GetError());
        ASSERT(!"Failed to read recorded input file for looped-live editing.");
    }
    else
    {
        state->loopState.loopedLiveEditingState = LoopedLiveEditingState::playing;
        RestoreMemoryBlocksFromFile(state->loopState.playbackHandle);
    }
}

local void SDLEndInputPlayback(SDLState* state)
{
    SDLClearBlocksByMask(state, sdlMem_freedDuringLoop);
    
    TRY(SDL_CloseIO(state->loopState.playbackHandle));
    state->loopState.loopedLiveEditingState = LoopedLiveEditingState::none;
}

local void SDLBeginRecordingInput(SDLState* state)
{
    const char* inputFilePath = SDLGetInputFilePath();
    state->loopState.recordingHandle = SDL_IOFromFile(inputFilePath, "w");
    if (state->loopState.recordingHandle == NULL)
    {
        LOG_ERROR(SDL_GetError());
        ASSERT(!"Failed to create recording handle for looped-live editing.");
    }
    else
    {
        state->loopState.loopedLiveEditingState = LoopedLiveEditingState::recording;
        WriteMemoryBlocksToFile(state, state->loopState.recordingHandle);
    }
    
}

local void SDLEndRecordingInput(SDLState* state)
{
    TRY(SDL_CloseIO(state->loopState.recordingHandle));
    state->loopState.loopedLiveEditingState = LoopedLiveEditingState::none;
}

local void SDLRecordStdString(SDLState* state, const std::string* str)
{
    u32 length = static_cast<u32>(str->size());
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, &length, sizeof(length)), sizeof(length));
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, str->data(), length), length);
}

local void SDLRecordStdSetOfString(SDLState* state, std::set<std::string>* strings)
{
    u32 count = static_cast<u32>(strings->size());
    TRY_EXPECT(SDL_WriteIO(state->loopState.recordingHandle, &count, sizeof(count)), sizeof(count));

    for (const std::string& str : *strings)
    {
        SDLRecordStdString(state, &str);
    }
}

local void SDLRecordInput(SDLState* state, GameInput* gameInput)
{
    // NOTE(marvin): The keys down is not going to be used.
    siz bytesWritten = SDL_WriteIO(state->loopState.recordingHandle, gameInput, sizeof(*gameInput));
    ASSERT(bytesWritten == sizeof(*gameInput) && "Failed to properly write.");

    // NOTE(marvin): Have to manually deserialize std set.
    SDLRecordStdSetOfString(state, &gameInput->keysDownPrevFrame);
    SDLRecordStdSetOfString(state, &gameInput->keysDownThisFrame);
}

local void SDLPlaybackStdString(SDLState* state, std::set<std::string>* strings)
{
    u32 length;
    TRY_EXPECT(SDL_ReadIO(state->loopState.playbackHandle, &length, sizeof(length)), sizeof(length));

    std::string str(length, '\0');
    TRY_EXPECT(SDL_ReadIO(state->loopState.playbackHandle, str.data(), length), length);
    strings->insert(std::move(str));
}

local void SDLPlaybackStdSetOfString(SDLState* state, std::set<std::string>* strings)
{
    u32 count;
    ASSERT(SDL_ReadIO(state->loopState.playbackHandle, &count, sizeof(count)) == sizeof(count));

    for (u32 index = 0; index < count; ++index)
    {
        SDLPlaybackStdString(state, strings);
    }
}

local void SDLPlaybackInput(SDLState* state, GameInput* gameInput)
{
    // NOTE(marvin): Would it be possible for there to be two notion
    // of mouse? One used in the game, and one for interacting with
    // the editor?

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
    }
    ASSERT(bytesRead == sizeof(*gameInput));
    
    new (&gameInput->keysDownPrevFrame) std::set<std::string>();
    new (&gameInput->keysDownThisFrame) std::set<std::string>();
    SDLPlaybackStdSetOfString(state, &gameInput->keysDownPrevFrame);
    SDLPlaybackStdSetOfString(state, &gameInput->keysDownThisFrame);
}

// >>> Global Interface <<<
void ToggleLoopedLiveEditingState(SDLState* state)
{
    switch (state->loopState.loopedLiveEditingState)
    {
      case LoopedLiveEditingState::none:
      {
          SDLBeginRecordingInput(state);
      } break;
      case LoopedLiveEditingState::recording:
      {
          SDLEndRecordingInput(state);
          SDLBeginInputPlayback(state);
      } break;
      case LoopedLiveEditingState::playing:
      {
          SDLEndInputPlayback(state);
      } break;
    }
}

void ProcessInputWithLooping(SDLState* state, GameInput* gameInput) {
    switch (state->loopState.loopedLiveEditingState)
    {
      case LoopedLiveEditingState::none: {} break;
      case LoopedLiveEditingState::recording:
      {
          SDLRecordInput(state, gameInput);
      } break;
      case LoopedLiveEditingState::playing:
      {
          SDLPlaybackInput(state, gameInput);
      } break;
    }
}

bool SetFlagAllocatedIfInLoop(SDLState* state, SDLMemoryFlags* flag) {
    if (SDLIsInLoop(state)) {
        *flag = sdlMem_allocatedDuringLoop;
        return true;
    }
    *flag = sdlMem_none;
    return true;
}
bool SetFlagFreedIfInLoop(SDLState* state, SDLMemoryFlags* flag) {
    if (SDLIsInLoop(state)) {
        if (*flag == sdlMem_none)
        {
            *flag = sdlMem_freedDuringLoop;
        }
        return true;
    }
    return true;
}
#else
bool SDLIsInLoop(SDLState* state) { return false; }
void ToggleLoopedLiveEditingState(SDLState* state) {}
void ProcessInputWithLooping(SDLState* state, GameInput* gameInput) {}
bool SetFlagAllocatedIfInLoop(SDLState* state, SDLMemoryFlags* flag) {return false;}
bool SetFlagFreedIfInLoop(SDLState* state, SDLMemoryFlags* flag) {return false;}
#endif

