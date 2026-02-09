#pragma once

// This file controls how the editor controls live loop editing.
// Allows the storing of editor 

#include <SDL3/SDL.h>

#include <set>

#include <meta_definitions.h>

struct SDLState;
struct GameInput;
struct SDLMemoryBlock;

// Represents the state of a memory block durring looping
struct LoopMemoryFlags {
private:
    enum SDLMemoryFlags
    {
        sdlMem_none = 0b0,
        sdlMem_allocatedDuringLoop = 0b1,
        sdlMem_freedDuringLoop = 0b10,
    };
    SDLMemoryFlags m_flags{ sdlMem_none };

public:
    friend class LoopUtils;
};

// Represents the state of looping and stored input information
// For looping.
struct LoopState {
private: 
    enum class LoopedLiveEditingState : u8
    {
        none,
        recording,
        playing,
    };
    LoopedLiveEditingState loopedLiveEditingState{ LoopedLiveEditingState::none };
    union
    {
        // NOTE(marvin): A flat sequence of [GameInput, u32 (for
        // number of elements in keysDown set), a sequence of [u32
        // (for number of elements in string), characters that make
        // the string]]
        SDL_IOStream* recordingHandle{ nullptr };
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
    static const char* SDLGetInputFilePath();
    static b8 SDLIsInLoop(const SDLState* state);
    static void SDLClearBlocksByMask(SDLState* state, LoopMemoryFlags::SDLMemoryFlags mask);
    static void SDLBeginInputPlayback(SDLState* state);
    static void SDLEndInputPlayback(SDLState* state);
    static void SDLBeginRecordingInput(SDLState* state);
    static void SDLEndRecordingInput(SDLState* state);
    static void SDLRecordStdString(SDLState* state, const std::string* str);
    static void SDLRecordStdSetOfString(SDLState* state, const std::set<std::string>* strings);
    static void SDLRecordInput(SDLState* state, const GameInput* gameInput);
    static void SDLPlaybackStdString(SDLState* state, std::set<std::string>* strings);
    static void SDLPlaybackStdSetOfString(SDLState* state, std::set<std::string>* strings);

    // Returns if a reset happen?
    static b8 SDLPlaybackInput(SDLState* state, GameInput* gameInput);

public:
    static b8 GetIsStateInLoop(const SDLState* state);

    static void ToggleLoopedLiveEditingState(SDLState* state);

    // Based on live editing state, stores or plays game input
    // Produces a boolean on whether to reload game code.
    // TODO(marvin): Producing a b8ean for ProcessInputWithLooping feels very contrived. Keep eyes opened for a better way.
    static b8 ProcessInputWithLooping(SDLState* state, GameInput* gameInput);

    // Gets and sets allocation flags
    static void SetBlockFlagLoopAllocated(SDLMemoryBlock* flag);
    static void SetBlockFlagLoopFreed(SDLMemoryBlock* flag);
    static void SetBlockFlagLoopNone(SDLMemoryBlock* flag);

    static b8 GetBlockFlagLoopAllocated(const SDLMemoryBlock* flag);
};
