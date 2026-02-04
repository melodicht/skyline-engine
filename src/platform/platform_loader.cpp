#include <platform_loader.h>

#include <format>

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


void GameCode::loadGameCode(SDL_Time newFileLastWritten, b32 editor)
{
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
    gameCodeUseFilePath = std::format(GAME_CODE_USE_FILE_NAME "_{}_{}.dll", tag.c_str(), m_reloadCount);
#else
    gameCodeUseFilePath = std::format("./lib" GAME_CODE_USE_FILE_NAME "_{}_{}.so", tag.c_str(), m_reloadCount);
#endif

    // NOTE(marvin): Need to have a copy for the platform executable
    // to use so that when recompile, allowed to rewrite the source
    // without it being locked by the platform executable.
    if(!SDL_CopyFile(gameCodeSrcFilePath, gameCodeUseFilePath.c_str()))
    {
        LOG_ERROR("Unable to copy game module source to used.");
        LOG_ERROR(SDL_GetError());
    }
    m_sharedObjectHandle = SDL_LoadObject(gameCodeUseFilePath.c_str());
    if (!m_sharedObjectHandle)
    {
        LOG_ERROR("Game code loading failed.");
        LOG_ERROR(SDL_GetError());
    }

    m_gameInitializePtr = (game_initialize_t *)SDL_LoadFunction(m_sharedObjectHandle, "GameInitialize");
    m_gameLoadPtr = (game_load_t *)SDL_LoadFunction(m_sharedObjectHandle, "GameLoad");
    m_gameUpdateAndRenderPtr = (game_update_and_render_t *)SDL_LoadFunction(m_sharedObjectHandle, "GameUpdateAndRender");
    if (m_gameInitializePtr && m_gameLoadPtr && m_gameUpdateAndRenderPtr)
    {
        m_fileLastWritten = newFileLastWritten;
    }
    else
    {
        LOG_ERROR("Unable to load symbols from game shared object.");
        m_gameInitializePtr = 0;
        m_gameLoadPtr = 0;
        m_gameUpdateAndRenderPtr = 0;

    }
}

void GameCode::loadGameCode(b32 editor)
{
    const char *gameCodeSrcFilePath = getGameCodeSrcFilePath();
    loadGameCode(getFileLastWritten(gameCodeSrcFilePath), editor);
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
    m_gameUpdateAndRenderPtr = 0;
    m_reloadCount++;
}

b32 GameCode::hasGameCodeChanged()
{
    b32 result;
    const char *gameCodeSrcFilePath = getGameCodeSrcFilePath();
    m_fileNewLastWritten_ = getFileLastWritten(gameCodeSrcFilePath);

    if (m_fileNewLastWritten_)
    {
        result = m_fileNewLastWritten_ > m_fileLastWritten;
    }

    return result;
}

GameCode::GameCode(bool editor) {
    const char* joltLibSrcFilePath = getJoltLibSrcFilePath();
    SDL_SharedObject* joltSharedObjectHandle = SDL_LoadObject(joltLibSrcFilePath);
    if (!joltSharedObjectHandle)
    {
        LOG_ERROR("Jolt loading failed.");
        LOG_ERROR(SDL_GetError());
    }
   loadGameCode(editor);
}
void GameCode::updateGameCode(GameMemory& memory, b32 hasEditor) {
    if (hasGameCodeChanged())
    {
        unloadGameCode();
        loadGameCode(m_fileNewLastWritten_, hasEditor);
        gameLoad(memory, hasEditor, true);
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