#pragma once

// This file controls how the editor controls live loop editing.
// Allows the storing of editor 

#include <SDL3/SDL.h>
#include <span>
#include <vector>

#include <input.h>

#include <meta_definitions.h>
#include <game_platform.h>

struct SDLState;
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
    // This checkpoint lives outside tracked game memory, just like the live
    // accumulated input. Never restore either object's owning pointers as bytes.
    std::vector<PlayerData> recordedPlayerData;
    std::vector<PlayerData> recordedAccumulatedPlayerData;
    // Holds one frame on its way to or from the recording file.
    std::vector<std::byte> frameBuffer;
    union
    {
        // NOTE(marvin): A flat sequence of frames, each being [u32 (for
        // size of the rest of the frame), the
        // InputSpecification::WriteBuffer output of each player]
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
    static void SDLBeginInputPlayback(SDLState* state, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData);
    static void SDLEndInputPlayback(SDLState* state);
    static void SDLBeginRecordingInput(SDLState* state, std::span<const PlayerData> playerData, const std::vector<PlayerData>* accumulatedPlayerData);
    static void SDLEndRecordingInput(SDLState* state);
    static void SDLRecordInput(SDLState* state, const InputSpecification& specification, std::span<const PlayerData> playerData);

    // Returns if a reset happen?
    static b8 SDLPlaybackInput(SDLState* state, const InputSpecification& specification, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData, b8 forceReloadGameCode);

public:
    static b8 GetIsStateInLoop(const SDLState* state);

    static b8 GetIsStateInPlayback(const SDLState* state);

    static void ToggleLoopedLiveEditingState(SDLState* state, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData);

    // Based on live editing state, stores or plays game input
    // Produces a boolean on whether to reload game code.
    // TODO(marvin): Producing a b8ean for ProcessInputWithLooping feels very contrived. Keep eyes opened for a better way.
    static b8 ProcessInputWithLooping(SDLState* state, const InputSpecification& specification, std::span<PlayerData> playerData, std::vector<PlayerData>* accumulatedPlayerData, b8 forceReloadGameCode);

    // Gets and sets allocation flags
    static void SetBlockFlagLoopAllocated(SDLMemoryBlock* flag);
    static void SetBlockFlagLoopFreed(SDLMemoryBlock* flag);
    static void SetBlockFlagLoopNone(SDLMemoryBlock* flag);

    static b8 GetBlockFlagLoopAllocated(const SDLMemoryBlock* flag);
};
