#if defined(__unix__) || defined(__unix) || defined(unix) ||    \
    (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM_UNIX
#elif defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#endif

#include <format>
#include <filesystem>
#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#if SKL_ENABLED_EDITOR
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#endif

#if EMSCRIPTEN
#include <emscripten/html5.h>
#endif

#define GAME_CODE_SRC_FILE_NAME "game-module"
#define GAME_CODE_USE_FILE_NAME "game-module-locked"

#include <game_platform.h>
#include <render_backend.h>

struct SDLGameCode
{
    SDL_SharedObject *sharedObjectHandle;

    SDL_Time fileLastWritten;
    SDL_Time fileNewLastWritten_;

    game_initialize_t *gameInitialize;
    game_load_t *gameLoad;
    game_update_and_render_t *gameUpdateAndRender;
};

struct AppInformation
{
    SDL_Window *window;
    SDLGameCode &gameCode;
    GameMemory &gameMemory;
    SDL_Event &e;
    bool playing;
    u64 now;
    u64 last;
    b32 editor;

    AppInformation(SDL_Window *setWindow, SDLGameCode &gameCode, GameMemory &gameMemory, SDL_Event &setE, bool setPlaying, u64 setNow, u64 setLast, b32 setEditor) :
        window(setWindow),
        gameCode(gameCode),
        gameMemory(gameMemory),
        e(setE),
        playing(setPlaying),
        now(setNow),
        last(setLast),
        editor(setEditor)
    { }
};

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 1200

file_global std::set<std::string> keysDown;
file_global f32 mouseDeltaX = 0;
file_global f32 mouseDeltaY = 0;

file_global f32 mouseX = 0;
file_global f32 mouseY = 0;

file_global u32 reloadCount = 0;

local const char *SDLGetGameCodeSrcFilePath()
{
    const char *result;
#ifdef PLATFORM_WINDOWS
    result = GAME_CODE_SRC_FILE_NAME ".dll";
#else
    result = "./lib" GAME_CODE_SRC_FILE_NAME ".so";
#endif
    return result;
}

local inline SDL_Time SDLGetFileLastWritten(const char *path)
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

local SDLGameCode SDLLoadGameCode(SDL_Time newFileLastWritten)
{
    SDLGameCode result = {};
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    // NOTE(marvin): Could make a macro to generalize, but lazy and
    // unsure of impact on compile time.
    std::string gameCodeUseFilePath;
#ifdef PLATFORM_WINDOWS
    gameCodeUseFilePath = std::format(GAME_CODE_USE_FILE_NAME "{}.dll", reloadCount);
#else
    gameCodeUseFilePath = std::format("./lib" GAME_CODE_USE_FILE_NAME "{}.so", reloadCount);
#endif

    // NOTE(marvin): Need to have a copy for the platform executable
    // to use so that when recompile, allowed to rewrite the source
    // without it being locked by the platform executable.
    if(!SDL_CopyFile(gameCodeSrcFilePath, gameCodeUseFilePath.c_str()))
    {
        LOG_ERROR("Unable to copy game module source to used.");
        LOG_ERROR(SDL_GetError());
    }
    result.sharedObjectHandle = SDL_LoadObject(gameCodeUseFilePath.c_str());
    if (!result.sharedObjectHandle)
    {
        LOG_ERROR("Game code loading failed.");
        LOG_ERROR(SDL_GetError());
    }

    result.gameInitialize = (game_initialize_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameInitialize");
    result.gameLoad = (game_load_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameLoad");
    result.gameUpdateAndRender = (game_update_and_render_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameUpdateAndRender");
    if (result.gameInitialize && result.gameLoad && result.gameUpdateAndRender)
    {
        result.fileLastWritten = newFileLastWritten;
    }
    else
    {
        LOG_ERROR("Unable to load symbols from game shared object.");
        result.gameInitialize = 0;
        result.gameLoad = 0;
        result.gameUpdateAndRender = 0;

    }
    return result;
}

local SDLGameCode SDLLoadGameCode()
{
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    return SDLLoadGameCode(SDLGetFileLastWritten(gameCodeSrcFilePath));
}

local void SDLUnloadGameCode(SDLGameCode *gameCode)
{
    if(gameCode->sharedObjectHandle)
    {
        SDL_UnloadObject(gameCode->sharedObjectHandle);
        gameCode->sharedObjectHandle = 0;
    }
    gameCode->gameInitialize = 0;
    gameCode->gameLoad = 0;
    gameCode->gameUpdateAndRender = 0;
    reloadCount++;
}

local b32 SDLGameCodeChanged(SDLGameCode *gameCode)
{
    b32 result;
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    gameCode->fileNewLastWritten_ = SDLGetFileLastWritten(gameCodeSrcFilePath);

    if (gameCode->fileNewLastWritten_)
    {
        result = gameCode->fileNewLastWritten_ > gameCode->fileLastWritten;
    }

    return result;
}

void updateLoop(void* appInfo) {
    AppInformation* info = (AppInformation* )appInfo;
    info->last = info->now;
    info->now = SDL_GetPerformanceCounter();

    f32 deltaTime = (f32)((info->now - info->last) / (f32)SDL_GetPerformanceFrequency());

    SDLGameCode &gameCode = info->gameCode;
    if (SDLGameCodeChanged(&gameCode))
    {
        SDLUnloadGameCode(&gameCode);
        info->gameCode = SDLLoadGameCode(gameCode.fileNewLastWritten_);
        gameCode = info->gameCode;
        gameCode.gameLoad(info->gameMemory, info->editor);
    }

    while (SDL_PollEvent(&info->e))
    {
        // Cut off Imgui until we actually implement a base renderer for WGPU
        #if SKL_ENABLED_EDITOR
        ImGui_ImplSDL3_ProcessEvent(&info->e);
        #endif
        switch (info->e.type)
        {
            case SDL_EVENT_QUIT:
                info->playing = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (info->e.key.key == SDLK_ESCAPE)
                {
                    info->playing = false;
                }
                keysDown.insert(SDL_GetKeyName(info->e.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                keysDown.erase(SDL_GetKeyName(info->e.key.key));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                keysDown.insert(std::format("Mouse {}", info->e.button.button));
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                keysDown.erase(std::format("Mouse {}", info->e.button.button));
                break;
        }
    }

    SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);
    SDL_GetMouseState(&mouseX, &mouseY);

    s32 windowWidth = WINDOW_WIDTH;
    s32 windowHeight = WINDOW_HEIGHT;
    SDL_GetWindowSize(info->window, &windowWidth, &windowHeight);

    // Cut off Imgui until we actually implement a base renderer for WGPU
    #if SKL_ENABLED_EDITOR
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    
    if (io.WantCaptureMouse)
    {
        keysDown.erase("Mouse 1");
    }
    #endif

    GameInput gameInput;
    gameInput.mouseDeltaX = mouseDeltaX;
    gameInput.mouseDeltaY = mouseDeltaY;
    gameInput.mouseX = mouseX;
    gameInput.mouseY = mouseY;
    gameInput.keysDown = keysDown;
    gameCode.gameUpdateAndRender(info->gameMemory, gameInput, deltaTime);

    mouseDeltaX = 0;
    mouseDeltaY = 0;

    f32 msPerFrame =  1000.0f * deltaTime;
    f32 fps = 1 / deltaTime;
    //printf("%.02f ms/frame (FPS: %.02f)\n", msPerFrame, fps);
    return;
}


int main(int argc, char** argv)
{
    std::cout << "Current path: " << std::filesystem::current_path() << std::endl;
    srand(static_cast<unsigned>(time(0)));

    SDL_Window *window = NULL;
    SDL_Surface *screenSurface = NULL;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Skyline Engine", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | GetRenderWindowFlags());
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    #if SKL_ENABLED_EDITOR
    ImGuiContext *imGuiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplSDL3_InitForOther(window);
    #endif

    std::string mapName = "test";
    bool editor = false;

    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "-editor"))
        {
            editor = true;
        }
        if ((!strcmp(argv[i], "-map")) && ((++i) < argc))
        {
            mapName = argv[i];
        }
    }

    if (!editor)
    {
        SDL_SetWindowRelativeMouseMode(window, true);
    }


    RenderInitInfo initDesc
    {
        .window = window,
        .startWidth = WINDOW_WIDTH,
        .startHeight = WINDOW_HEIGHT,
        .editor = editor
    };
    InitRenderer(initDesc);

    SDLGameCode gameCode = SDLLoadGameCode();
    GameMemory gameMemory = {};
    gameMemory.permanentStorageSize = Megabytes(512 + 128);
    gameMemory.permanentStorage = SDL_malloc(static_cast<size_t>(gameMemory.permanentStorageSize));
#if SKL_INTERNAL
    gameMemory.debugStorageSize = Megabytes(256 + 128);
    gameMemory.debugStorage = SDL_malloc(static_cast<size_t>(gameMemory.debugStorageSize));

    if (!gameMemory.debugStorage)
    {
        printf("SDL_malloc failed! SDL_Error: %s\n", SDL_GetError());
        Assert(false);
    }
#endif
    gameMemory.imGuiContext = imGuiContext;
    gameMemory.platformAPI.assetUtils = constructPlatformAssetUtils();
    gameMemory.platformAPI.renderer = constructPlatformRenderer();
    gameCode.gameLoad(gameMemory, editor);
    gameCode.gameInitialize(gameMemory, mapName, editor);

    SDL_Event e;
    bool playing = true;

    u64 now = SDL_GetPerformanceCounter();
    u64 last = 0;
    AppInformation app = AppInformation(window, gameCode, gameMemory, e, playing, now, last, editor);
    #if EMSCRIPTEN
    emscripten_set_main_loop_arg(
        [](void* userData) {
            updateLoop(userData);
        }, 
        (void*)&app, 
        0, true
    );
    #else
    while (app.playing)
    {
        updateLoop(&app);
    }
    #endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
