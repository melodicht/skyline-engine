#pragma once

#include <meta_definitions.h>
#include <game_platform.h>

#if !SKL_STATIC_MONOLITHIC
#include <platform_memory.h>

#define GAME_CODE_SRC_FILE_NAME "game-module"
#define GAME_CODE_USE_FILE_NAME "game-module-locked"

#define JOLT_LIB_SRC_FILE_NAME "Jolt"
#endif

// NOTE(marvin): Load is for setting up infrastructure, anything that
// has to be done after the game module is loaded in, including after
// hot reloads, whereas initialize is anything has to be done at the
// start ONCE. Thus, load happens before game initialize on game boot,
// and also before game update and render on hot reload (obviously).

// Represents the logic of loading the game module 
// from the platform.
class GameCode
{
private:
    #if !SKL_STATIC_MONOLITHIC
    // >>> Game code with hot reloading logic <<<
    SDL_SharedObject *m_sharedObjectHandle{ nullptr };

    SDL_Time m_fileLastWritten{ 0 };
    SDL_Time m_fileNewLastWritten{ 0 };

    game_initialize_t *m_gameInitializePtr{ nullptr };
    game_load_t *m_gameLoadPtr{ nullptr };
    game_update_and_render_t *m_gameUpdateAndRenderPtr{ nullptr };

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
    #endif

    // >>> No data store needed for static monolithic build <<< 

public:
    // >>> Common public interface <<<
    GameCode(bool editor);

    // Checks for any changes made to game code and attempts
    // hot reload if any changes are found. 
    // Does not do anything if SKL_STATIC_MONOLITHIC isn't turned on.
    void UpdateGameCode(GameMemory& memory, b8 hasEditor);

    // Basic game loop code
    GAME_LOAD(Load);

    GAME_INITIALIZE(Initialize);

    GAME_UPDATE_AND_RENDER(UpdateAndRender);
};
