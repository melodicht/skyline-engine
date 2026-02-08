#pragma once

// This file controls how the editor controls live loop editing.
// Allows the storing of editor 

#include <SDL3/SDL.h>

#include <set>

#include <meta_definitions.h>

struct SDLState;
struct GameInput;
struct SDLMemoryBlock;

// Represent 
struct LoopMemoryFlags {
private:
    enum SDLMemoryFlags
    {
        sdlMem_none = 0b0,
        sdlMem_allocatedDuringLoop = 0b1,
        sdlMem_freedDuringLoop = 0b10,
    };
    SDLMemoryFlags m_flags;

public:
    friend class LoopUtils;
};

// Represents 
struct LoopState {
private: 
    enum class LoopedLiveEditingState : u8
    {
        none,
        recording,
        playing,
    };
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

public:
    friend class LoopUtils;
};

// This is the sole access point for game loop logic.
// This structure ensures that general code will not be able to access looping logic
// ensuring tight encapsulation.
class LoopUtils {
private:
    file_global const char* SDLGetInputFilePath();
    file_global b8 SDLIsInLoop(const SDLState* state);
    file_global void SDLClearBlocksByMask(SDLState* state, LoopMemoryFlags::SDLMemoryFlags mask);
    file_global void SDLBeginInputPlayback(SDLState* state);
    file_global void SDLEndInputPlayback(SDLState* state);
    file_global void SDLBeginRecordingInput(SDLState* state);
    file_global void SDLEndRecordingInput(SDLState* state);
    file_global void SDLRecordStdString(SDLState* state, const std::string* str);
    file_global void SDLRecordStdSetOfString(SDLState* state, const std::set<std::string>* strings);
    file_global void SDLRecordInput(SDLState* state, const GameInput* gameInput);
    file_global void SDLPlaybackStdString(SDLState* state, std::set<std::string>* strings);
    file_global void SDLPlaybackStdSetOfString(SDLState* state, std::set<std::string>* strings);

    // Returns if a reset happen?
    file_global b8 SDLPlaybackInput(SDLState* state, GameInput* gameInput);

public:
    // Checks whether SDL is 
    file_global b8 GetIsStateInLoop(const SDLState* state);

    file_global void ToggleLoopedLiveEditingState(SDLState* state);

    // Based on live editing state, stores or plays game input
    // Produces a boolean on whether to reload game code.
    // TODO(marvin): Producing a b8ean for ProcessInputWithLooping feels very contrived. Keep eyes opened for a better way.
    file_global b8 ProcessInputWithLooping(SDLState* state, GameInput* gameInput);

    // Gets and sets allocation flags
    file_global void SetBlockFlagLoopAllocated(SDLMemoryBlock* flag);
    file_global void SetBlockFlagLoopFreed(SDLMemoryBlock* flag);
    file_global void SetBlockFlagLoopNone(SDLMemoryBlock* flag);

    file_global b8 GetBlockFlagLoopAllocated(const SDLMemoryBlock* flag);
};
