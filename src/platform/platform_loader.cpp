#include <platform_loader.h>

#include <format>

#define PATH_BUFFER_COUNT 8

const char* GameCode::getGameCodeSrcFilePath()
{
    const char* result;
#if defined(PLATFORM_WINDOWS)
    result = GAME_CODE_SRC_FILE_NAME ".dll";
#else
    result = "./lib" GAME_CODE_SRC_FILE_NAME ".so";
#endif
    return result;
}

const char* GameCode::getJoltLibSrcFilePath()
{
    const char* result;
#if defined(PLATFORM_WINDOWS)
    result = JOLT_LIB_SRC_FILE_NAME ".dll";
#elif defined(PLATFORM_APPLE)
    result = "./lib" JOLT_LIB_SRC_FILE_NAME ".dylib";
#else
    result = "./lib" JOLT_LIB_SRC_FILE_NAME ".so";
#endif
    return result;
}

inline SDL_Time GameCode::getFileLastWritten(const char *path)
{
    SDL_Time result;
    SDL_PathInfo pathInfo;
    if (SDL_GetPathInfo(path, &pathInfo))
    {
        result = pathInfo.modify_time;
    }
    else
    {
        LOG_ERROR("Unable to get path info of game code.");
    }
    return result;
}


b8 GameCode::loadGameCode(SDL_Time newFileLastWritten, b8 editor)
{
    // Temporary objects only fully loaded in on confirmation of successful load
    SDL_SharedObject* tempSharedHandle;
    game_initialize_t* tempGameInitializePtr;
    game_load_t* tempGameLoadPtr;
    game_get_persistent_dll_paths_t* tempGameGetPersistentDLLPathsPtr;
    game_update_and_render_t* tempGameUpdateAndRenderPtr;

    std::string tag = "game";
    if (editor)
    {
        tag = "editor";
    }
    const char *gameCodeSrcFilePath = getGameCodeSrcFilePath();
    // NOTE(marvin): Could make a macro to generalize, but lazy and
    // unsure of impact on compile time.
    std::string gameCodeUseFilePath;
#if defined(PLATFORM_WINDOWS)
    gameCodeUseFilePath = std::format(GAME_CODE_USE_FILE_NAME "_{}_{}.dll", tag.c_str(), m_loadCount);
#else
    gameCodeUseFilePath = std::format("./lib" GAME_CODE_USE_FILE_NAME "_{}_{}.so", tag.c_str(), m_loadCount);
#endif

    // NOTE(marvin): Need to have a copy for the platform executable
    // to use so that when recompile, allowed to rewrite the source
    // without it being locked by the platform executable.
    if(!SDL_CopyFile(gameCodeSrcFilePath, gameCodeUseFilePath.c_str()))
    {
        LOG_ERROR("Unable to copy game module source to used.");
        LOG_ERROR(SDL_GetError());
    }
    tempSharedHandle = SDL_LoadObject(gameCodeUseFilePath.c_str());
    if (!tempSharedHandle)
    {
        LOG_ERROR("Game code loading failed.");
        LOG_ERROR(SDL_GetError());
        SDL_RemovePath(gameCodeUseFilePath.c_str());
    }

    tempGameInitializePtr = (game_initialize_t *)SDL_LoadFunction(tempSharedHandle, "GameInitialize");
    tempGameLoadPtr = (game_load_t *)SDL_LoadFunction(tempSharedHandle, "GameLoad");
    tempGameGetPersistentDLLPathsPtr = (game_get_persistent_dll_paths_t *)SDL_LoadFunction(tempSharedHandle, "GameGetPersistentDLLPaths");
    tempGameUpdateAndRenderPtr = (game_update_and_render_t *)SDL_LoadFunction(tempSharedHandle, "GameUpdateAndRender");
    if (tempGameInitializePtr && tempGameLoadPtr && tempGameGetPersistentDLLPathsPtr && tempGameUpdateAndRenderPtr)
    {
        m_fileLastWritten = newFileLastWritten;
        if (m_sharedObjectHandle) {
            unloadGameCode();
        }
        m_sharedObjectHandle = tempSharedHandle;
        m_gameInitializePtr =  tempGameInitializePtr;
        m_gameLoadPtr = tempGameLoadPtr;
        m_gameGetPersistentDLLPathsPtr = tempGameGetPersistentDLLPathsPtr;
        m_gameUpdateAndRenderPtr = tempGameUpdateAndRenderPtr;
        m_loadCount++;
        return true;
    }
    else
    {
        LOG_ERROR("Unable to load symbols from game shared object.");
        if (tempSharedHandle)
        {
            SDL_UnloadObject(tempSharedHandle);
        }
        return false;
    }
}

b8 GameCode::loadGameCode(b8 editor)
{
    const char *gameCodeSrcFilePath = getGameCodeSrcFilePath();
    return loadGameCode(getFileLastWritten(gameCodeSrcFilePath), editor);
}

void GameCode::unloadGameCode()
{
    if(m_sharedObjectHandle)
    {
        SDL_UnloadObject(m_sharedObjectHandle);        
        m_sharedObjectHandle = 0;
    }
    m_gameInitializePtr = 0;
    m_gameLoadPtr = 0;
    m_gameGetPersistentDLLPathsPtr = 0;
    m_gameUpdateAndRenderPtr = 0;
}

b8 GameCode::hasGameCodeChanged()
{
    const char *gameCodeSrcFilePath = getGameCodeSrcFilePath();
    SDL_PathInfo pathInfo;
    if (!SDL_GetPathInfo(gameCodeSrcFilePath, &pathInfo))
    {
        LOG_ERROR("Unable to get game code path info");
        return false;
    }
    if (!pathInfo.size)
    {
        return false;
    }

    m_fileNewLastWritten = pathInfo.modify_time;
    return m_fileNewLastWritten > m_fileLastWritten;
}

// NOTE(marvin): Have the platform hold onto the shared object so that
// it persists between hot reloads.
local void LoadSharedObject(const char* path)
{
    SDL_SharedObject* sharedObjectHandle = SDL_LoadObject(path);
    if (!sharedObjectHandle)
    {
        LOG_ERROR(path << " loading failed.");
        LOG_ERROR(SDL_GetError());
    }
}

GameCode::GameCode(bool editor) {
    const char* joltLibSrcFilePath = getJoltLibSrcFilePath();
    LoadSharedObject(joltLibSrcFilePath);

    loadGameCode(editor);

    const char* pathBuffer[PATH_BUFFER_COUNT] = {0};
    m_gameGetPersistentDLLPathsPtr(pathBuffer);

    for (u32 pathBufferIndex = 0; pathBufferIndex < PATH_BUFFER_COUNT; ++pathBufferIndex)
    {
        const char* path = pathBuffer[pathBufferIndex];
        if (path)
        {
            LoadSharedObject(path);
        }
    }
    
}
void GameCode::updateGameCode(GameMemory& memory, b8 hasEditor) {
    if (hasGameCodeChanged())
    {
        if (loadGameCode(m_fileNewLastWritten, hasEditor)) {
            gameLoad(memory, hasEditor, true);
        }
    }
}

GAME_LOAD(GameCode::gameLoad) {
    ASSERT(m_gameLoadPtr != nullptr);
    m_gameLoadPtr(memory, editor, gameInitialized);
}

GAME_INITIALIZE(GameCode::gameInitialize) {
    ASSERT(m_gameInitializePtr != nullptr);
    m_gameInitializePtr(memory, mapName, editor);
}

GAME_UPDATE_AND_RENDER(GameCode::gameUpdateAndRender) {
    ASSERT(m_gameUpdateAndRenderPtr != nullptr);
    m_gameUpdateAndRenderPtr(memory, input, frameTime);
}
