#pragma once

#include <platform_memory.h>
#include <game_platform.h>

#define GAME_CODE_SRC_FILE_NAME "game-module"
#define GAME_CODE_USE_FILE_NAME "game-module-locked"

#ifdef PLATFORM_WINDOWS
#define EXECUTABLE_FILE_NAME "skyline-engine.exe"
#else
#define EXECUTABLE_FILE_NAME "./skyline-engine"
#endif

#define JOLT_LIB_SRC_FILE_NAME "Jolt"

// NOTE(marvin): Load is for setting up infrastructure, anything that
// has to be done after the game module is loaded in, including after
// hot reloads, whereas initialize is anything has to be done at the
// start ONCE. Thus, load happens before game initialize on game boot,
// and also before game update and render on hot reload (obviously).

// Represents the logic of loading the game module 
// from the platform.
struct GameCode
{
private:
    // >>> Game code with hot reloading logic <<<
    SDL_SharedObject *m_sharedObjectHandle;

    SDL_Time m_fileLastWritten;
    SDL_Time m_fileNewLastWritten;

    game_initialize_t *m_gameInitializePtr;
    game_load_t *m_gameLoadPtr;
    game_update_and_render_t *m_gameUpdateAndRenderPtr;

    // The amount of time loadGameCode has been successfully run
    u32 m_loadCount{ 0 };

    const char* getGameCodeSrcFilePath();
    const char* getJoltLibSrcFilePath();
    inline SDL_Time getFileLastWritten(const char *path);

    // Returns whether the load was successful or not 
    b8 loadGameCode(SDL_Time newFileLastWritten, b8 editor);
    b8 loadGameCode(b8 editor);
    void unloadGameCode();
    b8 hasGameCodeChanged();

    // >>> Game code with static monolithic logic <<< 

public:
    // >>> Common public interface <<<
    GameCode(bool editor);

    // Checks for any changes made to game code and attempts
    // hot reload if any changes are found. 
    // Does not do anything if SKL_STATIC_MONOLITHIC isn't turned on.
    void updateGameCode(GameMemory& memory, b8 hasEditor);

    GAME_LOAD(gameLoad);

    GAME_INITIALIZE(gameInitialize);

    GAME_UPDATE_AND_RENDER(gameUpdateAndRender);
};